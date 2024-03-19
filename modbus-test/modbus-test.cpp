#include <iostream>
#include <cstring>

#ifndef _MSC_VER
#include <unistd.h>
#else
#include <winsock2.h>
#endif

#include <modbus.h>

// Function prototypes
int sendData(uint16_t data[], int address, modbus_t* connection);
int retrieveData(uint16_t data[], int address, modbus_t* connection);

int main() {

    // Setup connection
    modbus_t* ctx;
    ctx = modbus_new_tcp("127.0.0.1", 502);
    if (ctx == NULL) {
        std::cerr << "Failed to create Modbus context: " << modbus_strerror(errno) << std::endl;
        return -1;
    }

    if (modbus_connect(ctx) == -1) {
        std::cerr << "Connection failed: " << modbus_strerror(errno) << std::endl;
        modbus_free(ctx);
        return -1;
    }

    // System write variables per zone
    int setstand[5] = { 0, 0, 0, 0, 0 };
    int setauto[5] = { 0, 0, 0, 0, 0 };

    // System read variables per zone
    int level[5] = { 0, 0, 0, 0, 0 };
    int capaciteit[5] = { 0, 0, 0, 0, 0 };
    int energieverbr[5] = { 0, 0, 0, 0, 0 };
    int branduren[5] = { 0, 0, 0, 0, 0 };

    // Lamp ID's
    int lampIDs[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    int zone1 = { lampIDs[0], lampIDs[1] };

    // Prepare data to send
    uint16_t dataTx[4] = { level, capaciteit, energieverbr, branduren };
    int modbusStartAddressTx = 3000;

    for (;;) {

        // Send data
        if (sendData(dataTx, modbusStartAddressTx, ctx) == 0) {
            std::cout << "Data sent successfully." << std::endl;
        }

        // Prepare data to be retrieved
        uint16_t dataRx[2]; // Corrected declaration
        int modbusStartAddressRx = 3000;

        if (retrieveData(dataRx, modbusStartAddressRx, ctx) == 0) {
            std::cout << "Data read from Modbus registers:" << std::endl;
            for (int i = 0; i < 2; ++i) { // Changed the loop limit to 2
                std::cout << "Register " << i << ": " << dataRx[i] << std::endl; // Corrected accessing the array directly
            }
        }

    }

    // Close connection
    modbus_close(ctx);
    modbus_free(ctx);

    return 0;
}

int sendData(uint16_t data[], int address, modbus_t* connection) {
    int rc;
    // Write data to Modbus server
    rc = modbus_write_registers(connection, address, 4, data); // Corrected the number of registers
    if (rc == -1) {
        std::cerr << "Write error: " << modbus_strerror(errno) << std::endl;
        modbus_close(connection);
        modbus_free(connection);
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
        return -1;
    }
    return 0;
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
void errorHandling() {
    // Function for settings all the light to level 100%
}

// Set level of the light (0-100%)
void setLevel(int level, int lampID) {
    int lampLevel = (255 / 100) * level;
    // Rest of the code to set the lamp level
}

// Set level of the light (0-100%)
void toggleLamp(int state, int lampID) {
    // Rest of the code to set the lamp level
}


