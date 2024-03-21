#include <iostream>
#include <cstring>
#include <ctime>

#ifndef _MSC_VER
#include <unistd.h>
#else
#include <winsock2.h>
#endif

#include <modbus.h>

// Function prototypes
int sendData(uint16_t data[], int address, modbus_t* connection);
int retrieveData(uint16_t data[], int address, modbus_t* connection);
int getRunningHours(int address, modbus_t* connection);
int calcTime(time_t start);
int calcRunningHours(int currentRunMinutes);
void errorHandling(int lampIDs[]);
int calcPower(int currentLevel, int maxKW);
int calcCapicity(int level);
void setLevel(int level, int lampID);
void toggleLamp(int state, int lampID);
void updateLamps(int newLevel[], int newAuto[], int ids[]);

// Lamp ID's
int lampIDs[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

int main() {

    // Setup connection
    modbus_t* ctx;
    ctx = modbus_new_tcp("127.0.0.1", 502);
    if (ctx == NULL) {
        std::cerr << "Failed to create Modbus context: " << modbus_strerror(errno) << std::endl;
        errorHandling(lampIDs);
        return -1;
    }

    if (modbus_connect(ctx) == -1) {
        std::cerr << "Connection failed: " << modbus_strerror(errno) << std::endl;
        modbus_free(ctx);
        errorHandling(lampIDs);
        return -1;
    }

    // Start time in seconds
    time_t startTime = time(NULL);

    // system time
    int activeMinutes = 0;
    int prevActMinutes = 0;

    int maxW = 18; // W per zone

    // System write variables per zone
    int setstand[5] = { 0, 0, 0, 0, 0 };
    int setauto[5] = { 0, 0, 0, 0, 0 };

    // System read variables per zone
    int level[5] = { 1, 0, 0, 0, 0 };
    int capaciteit[5] = { 0, 0, 0, 0, 0 };
    int energieverbr[5] = { 0, 0, 0, 0, 0 };
    int branduren[5] = { 0, 0, 0, 0, 0 };

    // Start addresses for Tx & Rx
    int addressRx[5] = { 3000, 3006, 3012, 3018, 3024 };
    int addressTx[5] = { 3002, 3008, 3014, 3020, 3026 };

    // Get current running hours in minutes from the server
    for (int i = 0; i < 5; ++i) {
        branduren[i] = getRunningHours((addressTx[i] + 3), ctx);
       // std::cout << "Brand uren zone: " << (i + 1) << " Minuten: " << branduren[i] << " Address: " << (addressTx[i] + 3) << std::endl;
    }

    for (;;) {
        int activeMinutes = calcTime(startTime);
       // std::cout << "Active minutes: " << activeMinutes << std::endl;

        // Keep track of running hours (in minutes)
        for (int i = 0; i < 5; ++i) {
            if (level[i] != 0) {
                // Light is on, time tracking needed
                if (prevActMinutes != activeMinutes) {
                    branduren[i] = calcRunningHours(branduren[i]);
                }
                
                // Light is on, power consumption feedback is needed and convert it to kW
                energieverbr[i] = (calcPower(level[i], maxW) / 1000);
            }
            // Calculating the capacity
            capaciteit[i] = calcCapicity(level[i]);
           // std::cout << "Brand uren zone: " << (i + 1) << " Minuten: " << branduren[i] << " power: " << energieverbr[i] << " capaciteit: " << capaciteit[i]  << std::endl;
        }    

        if (prevActMinutes != activeMinutes) {
            prevActMinutes = activeMinutes;
        }

        //Sending & retrieveing data to and from the server
        for (int i = 0; i < 5; ++i) {
            // Prepare data to send
            uint16_t dataTx[4] = { level[i], capaciteit[i], energieverbr[i], branduren[i]};

            // Send data
            if (sendData(dataTx, addressTx[i], ctx) == 0) {
                std::cout << "Data sent successfully zone: " << (i+1) << std::endl;
            }
            else {
                errorHandling(lampIDs);
            }

            // Prepare data to be retrieved
            uint16_t dataRx[2];

            if (retrieveData(dataRx, addressRx[i], ctx) == 0) {
                std::cout << "Data read from Modbus registers zone: " << (i + 1) << std::endl;
                level[i] = dataRx[0]; //setstand[i] = dataRx[0];
                setauto[i] = dataRx[1];
            }
            else {
                errorHandling(lampIDs);
            }
        }

        // Check the light settings & update lamps
        updateLamps(level, setauto, lampIDs);

        //For loop needed for setting the level array correct based on the setauto
    }


    // Close connection
    modbus_close(ctx);
    modbus_free(ctx);

    return 0;
}

void updateLamps(int newLevel[], int newAuto[], int ids[]) {
    for (int i = 0; i < 5; ++i) {
        if (newAuto[i] == 1) {
            if (newLevel[i] > 0) {
                setLevel(newLevel[i], ids[i]);
                toggleLamp(1, ids[i]);
            }
            else {
                setLevel(newLevel[i], ids[i]);
                toggleLamp(0, ids[i]);
            }
        }
        else {
            // Default program (setlevel needs to be corrected to the right level)
            // Zone 1
            toggleLamp(1, ids[0]);
            toggleLamp(1, ids[1]);
            setLevel(100, ids[0]); 
            setLevel(100, ids[1]);

            // Zone 2
            toggleLamp(1, ids[2]);
            toggleLamp(1, ids[3]);
            setLevel(100, ids[2]);
            setLevel(100, ids[3]);

            // Zone 3
            toggleLamp(1, ids[4]);
            toggleLamp(1, ids[5]);
            setLevel(100, ids[4]);
            setLevel(100, ids[5]);

            // Zone 4
            toggleLamp(1, ids[6]);
            toggleLamp(1, ids[7]);
            setLevel(100, ids[6]);
            setLevel(100, ids[7]);

            // Zone 5
            toggleLamp(1, ids[8]);
            toggleLamp(1, ids[9]);
            setLevel(100, ids[8]);
            setLevel(100, ids[9]);
        }
    }
}

int sendData(uint16_t data[], int address, modbus_t* connection) {
    int rc;
    // Write data to Modbus server
    rc = modbus_write_registers(connection, address, 4, data); // Corrected the number of registers
    if (rc == -1) {
        std::cerr << "Write error: " << modbus_strerror(errno) << std::endl;
        modbus_close(connection);
        modbus_free(connection);
        errorHandling(lampIDs);
        return -1;
    }
    return 0;
}

int retrieveData(uint16_t data[], int address, modbus_t* connection) { // Corrected the parameter type
    int rc;
    /* Read 2 registers from the address */
    rc = modbus_read_registers(connection, address, 2, data); // Corrected passing the address of the pointer
    if (rc == -1) {
        std::cerr << "Read error: " << modbus_strerror(errno) << std::endl;
        modbus_close(connection);
        modbus_free(connection);
        errorHandling(lampIDs);
        return -1;
    }
    return 0;
}


// Function for getting the last running hours from the server
int getRunningHours(int address, modbus_t* connection) {
    int runningMinutes = 0;
    uint16_t dataRx[2]; 
    if (retrieveData(dataRx, address, connection) == 0) {
        std::cout << "Data read from Modbus registers:" << std::endl;
        for (int i = 0; i < 2; ++i) { // Changed the loop limit to 2
            std::cout << "Register " << i << ": " << dataRx[i] << std::endl; // Corrected accessing the array directly
        }
    }
    else {
        errorHandling(lampIDs);
    }
    return runningMinutes = dataRx[0];

}

// Calculate the time
int calcTime(time_t startTime) {
    // Calculate elapsed minutes
    time_t currentTime = time(NULL);
    int elapsedMinutes = (currentTime - startTime) / 60;
    // Sleep for 1 minute (60 seconds)
    // Note: This is not precise, but it's a simple way to introduce a delay
    // without external libraries
    for (int i = 0; i < 60; ++i) {
        // Introduce a small delay
        for (volatile int j = 0; j < 1000000; ++j) {}
    }

    return elapsedMinutes;
}

// Calculate the current running hours (in minutes)
int calcRunningHours(int currentRunMinutes) {
    int newRunMinutes = currentRunMinutes + 1;
    return newRunMinutes;
}

// Calculate the current power consumption
int calcPower(int currentLevel, int maxKW) {
    int currentKW = (maxKW / 100) * currentLevel;
    return currentKW;
}

// Function for calculating the current capacity
int calcCapicity(int level) {
    int currentCapacity = 100 - level;
    return currentCapacity;
}

// Error handling
void errorHandling(int lampIDs[]) {
    std::cerr << "FATAL ERROR" <<  std::endl;
    for (int i = 0; i < 5; ++i) {
        toggleLamp(1, lampIDs[i]);
        toggleLamp(1, lampIDs[i + 1]);
        setLevel(100, lampIDs[i]);
        setLevel(100, lampIDs[i+1]);
    }
}

// Set level of the light (0-100%)
void setLevel(int level, int lampID) {
    int lampLevel = (255 / 100) * level;
    // Rest of the code to set the lamp level
}

// Turn light off or on (0-1)
void toggleLamp(int state, int lampID) {
    // Rest of the code to set the lamp level
}


