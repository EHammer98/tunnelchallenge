#include <stdio.h>
#include <iostream>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "libmodbus/modbus.h"
#include <errno.h>
#include <chrono>
#include <thread>

using namespace std::chrono_literals;

#include "HueController.h"

// compile using: g++ modbus-linux.cpp HueController.cpp `pkg-config --cflags --libs libmodbus` -lcurl

// Function prototypes
uint16_t sendData(uint16_t data[], uint16_t address, modbus_t *connection);
uint16_t retrieveData(uint16_t data[], uint16_t address, modbus_t *connection);
uint16_t getRunningHours(uint16_t address, modbus_t *connection);
uint16_t calcTime(time_t start);
uint16_t calcRunningHours(uint16_t currentRunMinutes);
void errorHandling(uint16_t lampIDs[]);
uint16_t calcPower(uint16_t currentLevel, uint16_t maxKW);
uint16_t calcCapicity(uint16_t level);
void setLevel(uint16_t level, uint16_t lampID);
void toggleLamp(uint16_t state, uint16_t lampID);
void updateLamps(uint16_t newLevel[], uint16_t newAuto[], uint16_t ids[], uint16_t autoLevel[]);

// Lamp ID's
uint16_t lampIDs[16] = { 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22 };

// Prev. level
uint16_t prevLevel[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

HueController hueController("192.168.2.100");
const std::string modbusIP = "192.168.2.102";

int main()
{

    // HueController test

    // for (uint16_t i = 7; i < 13; i++)
    // {
    //     hueController.toggleLamp(true, i);
    // }

    // std::this_thread::sleep_for(1000ms);

    // for (uint16_t i = 7; i < 13; i++)
    // {
    //     hueController.toggleLamp(false, i);
    // }

    // std::this_thread::sleep_for(1000ms);
    // hueController.toggleLamp(true, 7);

    // for (uint16_t i = 1; i < 255; i += 5)
    // {
    //     hueController.setLevel(i, 7);
    //     std::this_thread::sleep_for(100ms);
    // }

    // for (uint16_t i = 255; i > 0; i -= 5)
    // {
    //     hueController.setLevel(i, 7);
    //     std::this_thread::sleep_for(100ms);
    // }

    // for (uint16_t i = 7; i < 13; i++)
    // {
    //     hueController.toggleLamp(false, i);
    // }

  

    // Setup connection
    modbus_t *ctx;
    ctx = modbus_new_tcp(modbusIP.c_str(), 502);
    if (ctx == NULL)
    {
        std::cerr << "Failed to create Modbus context: " << modbus_strerror(errno) << std::endl;
        errorHandling(lampIDs);
        return -1;
    }

    if (modbus_connect(ctx) == -1)
    {
        std::cerr << "Connection failed: " << modbus_strerror(errno) << std::endl;
        modbus_free(ctx);
        errorHandling(lampIDs);
        return -1;
    }

    // Start time in seconds
    time_t startTime = time(NULL);

    // system time
    uint16_t activeMinutes = 0;
    uint16_t prevActMinutes = 0;

    uint16_t maxW = 18; // W per zone

    // System write variables per zone
    uint16_t setstand[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    uint16_t setauto[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

    // System read variables per zone
    uint16_t level[8] = { 1, 0, 0, 0, 1, 0, 0, 0 };
    uint16_t autoLevel[8] = { 50, 25, 10, 25, 50, 25, 10, 25 };
    uint16_t capaciteit[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    uint16_t energieverbr[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    uint16_t branduren[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

    // Start addresses for Tx & Rx
    uint16_t addressOverride[8] = { 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000 };
    uint16_t addressRx[8] = { 3000, 3006, 3012, 3018, 3024, 3030, 3036, 3042 };
    uint16_t addressTx[8] = { 3002, 3008, 3014, 3020, 3026, 3032, 3038, 3044 };

    // Get current running hours in minutes from the server
    for (uint16_t i = 0; i < 8; ++i)
    {
        branduren[i] = getRunningHours((addressTx[i] + 3), ctx);
        std::cout << "Brand uren zone: " << (i + 1) << " Minuten: " << branduren[i] << " Address: " << (addressTx[i] + 3) << std::endl;
    }

    std::cout << "TEST " << std::endl;

    while (1)
    {
        uint16_t activeMinutes = calcTime(startTime);
        std::cout << "Active minutes: " << activeMinutes << std::endl;

        // Keep track of running hours (in minutes)
        for (uint16_t i = 0; i < 8; ++i)
        {
            if (level[i] != 0)
            {
                // Light is on, time tracking needed
                if (prevActMinutes != activeMinutes)
                {
                    branduren[i] = calcRunningHours(branduren[i]);
                }

                // Light is on, power consumption feedback is needed and convert it to kW
                energieverbr[i] = (calcPower(level[i], maxW) / 1000);
            }

            // Calculating the capacity
            capaciteit[i] = calcCapicity(level[i]);
            // std::cout << "Brand uren zone: " << (i + 1) << " Minuten: " << branduren[i] << " power: " << energieverbr[i] << " capaciteit: " << capaciteit[i]  << std::endl;
        }

        if (prevActMinutes != activeMinutes)
        {
            prevActMinutes = activeMinutes;
        }

        // Sending & retrieveing data to and from the server
        for (uint16_t i = 0; i < 8; ++i)
        {
            // Prepare data to send
            uint16_t dataTx[4] = {level[i], capaciteit[i], energieverbr[i], branduren[i]};

            // Send data
            if (sendData(dataTx, addressTx[i], ctx) == 0)
            {
                std::cout << "Data sent successfully zone: " << (i + 1) << std::endl;
            }
            else
            {
                errorHandling(lampIDs);
            }

            // Prepare data to be retrieved
            uint16_t dataRx[2];

            if (retrieveData(dataRx, addressOverride[i], ctx) == 0) {
                std::cout << "Data read from Modbus registers zone Override: " << (i + 1) << std::endl;
                level[i] = dataRx[0]; //setstand[i] = dataRx[0];
                setauto[i] = 1;
                // Check if the override setstand is not set (higher then 0)
                if (dataRx[0] == 0) {
                    std::cout << "OVERRIDE OFF" << std::endl;
                    if (retrieveData(dataRx, addressRx[i], ctx) == 0) {
                        std::cout << "Data read from Modbus registers zone: " << (i + 1) << std::endl;
                        level[i] = dataRx[0]; //setstand[i] = dataRx[0];
                        setauto[i] = dataRx[1];
                    }
                    else {
                        errorHandling(lampIDs);
                    }
                }
            }
            else {
                errorHandling(lampIDs);
            }
        }

        // For loop needed for setting the level array correct based on the setauto
        for (uint16_t i = 0; i < 8; ++i)
        {
            if (setauto[i] == 0)
            {
                level[i] = autoLevel[i];
            }
        }

        // Check the light settings & update lamps
        updateLamps(level, setauto, lampIDs, autoLevel);
    }

    // Close connection
    modbus_close(ctx);
    modbus_free(ctx);

    return 0;
}

void updateLamps(uint16_t newLevel[], uint16_t newAuto[], uint16_t ids[], uint16_t defLevel[])
{
    for (uint16_t i = 0; i < 8; ++i)
    {
        if (newAuto[i] == 1)
        {
            if (newLevel[i] > 0)
            {
                // Check if defLevel is equal to prevLevel
                if (newLevel[i] != prevLevel[i])
                {
                    setLevel(newLevel[i], ids[i]);
                    toggleLamp(1, ids[i]);

                    // Set the previous level
                    prevLevel[i] = newLevel[i];
                }
            }
            else
            {
                setLevel(newLevel[i], ids[i]);
                toggleLamp(0, ids[i]);
            }
        }
        else
        {
            // Check if defLevel is equal to prevLevel
            if (defLevel[i] != prevLevel[i])
            {
                // Default program (setlevel needs to be corrected to the right level)
                // Zone 1
                toggleLamp(1, ids[0]);
                toggleLamp(1, ids[1]);
                setLevel(defLevel[i], ids[0]);
                setLevel(defLevel[i], ids[1]);

                // Zone 2
                toggleLamp(1, ids[2]);
                toggleLamp(1, ids[3]);
                setLevel(defLevel[i], ids[2]);
                setLevel(defLevel[i], ids[3]);

                // Zone 3
                toggleLamp(1, ids[4]);
                toggleLamp(1, ids[5]);
                setLevel(defLevel[i], ids[4]);
                setLevel(defLevel[i], ids[5]);

                // Zone 4
                toggleLamp(1, ids[6]);
                toggleLamp(1, ids[7]);
                setLevel(defLevel[i], ids[6]);
                setLevel(defLevel[i], ids[7]);

                // Zone 5
                toggleLamp(1, ids[8]);
                toggleLamp(1, ids[9]);
                setLevel(defLevel[i], ids[8]);
                setLevel(defLevel[i], ids[9]);

                // Zone 6
                toggleLamp(1, ids[10]);
                toggleLamp(1, ids[11]);
                setLevel(defLevel[i], ids[10]);
                setLevel(defLevel[i], ids[11]);

                // Zone 7
                toggleLamp(1, ids[12]);
                toggleLamp(1, ids[13]);
                setLevel(defLevel[i], ids[12]);
                setLevel(defLevel[i], ids[13]);

                // Zone 8
                toggleLamp(1, ids[14]);
                toggleLamp(1, ids[15]);
                setLevel(defLevel[i], ids[14]);
                setLevel(defLevel[i], ids[15]);

                // Set the previous level
                prevLevel[i] = defLevel[i];
            }
        }
    }
}

uint16_t sendData(uint16_t data[], uint16_t address, modbus_t *connection)
{
    uint16_t rc;
    // Write data to Modbus server
    rc = modbus_write_registers(connection, address, 4, data); // Corrected the number of registers
    if (rc == -1)
    {
        std::cerr << "Write error: " << modbus_strerror(errno) << std::endl;
        modbus_close(connection);
        modbus_free(connection);
        errorHandling(lampIDs);
        return -1;
    }
    return 0;
}

uint16_t retrieveData(uint16_t data[], uint16_t address, modbus_t *connection)
{ // Corrected the parameter type
    uint16_t rc;
    /* Read 2 registers from the address */
    rc = modbus_read_registers(connection, address, 2, data); // Corrected passing the address of the pointer
    if (rc == -1)
    {
        std::cerr << "Read error: " << modbus_strerror(errno) << std::endl;
        modbus_close(connection);
        modbus_free(connection);
        errorHandling(lampIDs);
        return -1;
    }
    return 0;
}

// Function for getting the last running hours from the server
uint16_t getRunningHours(uint16_t address, modbus_t *connection)
{
    uint16_t runningMinutes = 0;
    uint16_t dataRx[2];
    if (retrieveData(dataRx, address, connection) == 0)
    {
        std::cout << "Data read from Modbus registers:" << std::endl;
        for (uint16_t i = 0; i < 2; ++i)
        {                                                                    // Changed the loop limit to 2
            std::cout << "Register " << i << ": " << dataRx[i] << std::endl; // Corrected accessing the array directly
        }
    }
    else
    {
        errorHandling(lampIDs);
    }
    return runningMinutes = dataRx[0];
}

// Calculate the time
uint16_t calcTime(time_t startTime)
{
    // Calculate elapsed minutes
    time_t currentTime = time(NULL);
    uint16_t elapsedMinutes = (currentTime - startTime) / 60;
    // Sleep for 1 minute (60 seconds)elapsedMinutes
    // Note: This is not precise, but it's a simple way to introduce a delay
    // without external libraries
    // for (uint16_t i = 0; i < 60; ++i)
    // {
    //     // // Introduce a small delay
    //     // for (volatile int j = 0; j < 1000000; ++j)
    //     // {
    //     // }
    // }

    return elapsedMinutes;
}

// Calculate the current running hours (in minutes)
uint16_t calcRunningHours(uint16_t currentRunMinutes)
{
    uint16_t newRunMinutes = currentRunMinutes + 1;
    return newRunMinutes;
}

// Calculate the current power consumption
uint16_t calcPower(uint16_t currentLevel, uint16_t maxKW)
{
    uint16_t currentKW = (maxKW / 100) * currentLevel;
    return currentKW;
}

// Function for calculating the current capacity
uint16_t calcCapicity(uint16_t level)
{
    uint16_t currentCapacity = 100 - level;
    return currentCapacity;
}

// Error handling
void errorHandling(uint16_t lampIDs[])
{
    std::cerr << "FATAL ERROR" << std::endl;
    for (uint16_t i = 0; i < 5; ++i)
    {
        toggleLamp(1, lampIDs[i]);
        toggleLamp(1, lampIDs[i + 1]);
        setLevel(100, lampIDs[i]);
        setLevel(100, lampIDs[i + 1]);
    }
}

// Set level of the light (0-100%)
void setLevel(uint16_t level, uint16_t lampID)
{
    std::cout << "setLevel()" << std::endl;
    uint16_t lampLevel = (255 / 100) * level;
    // Rest of the code to set the lamp level
    hueController.setLevel((int)level, (int)lampID);
}

// Turn light off or on (0-1)
void toggleLamp(uint16_t state, uint16_t lampID)
{
    std::cout << "toggleLamp()" << std::endl;
    // Rest of the code to set the lamp level
    hueController.toggleLamp((bool)state, (int)lampID);
}
