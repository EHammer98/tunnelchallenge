#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <modbus.h>
#include <errno.h>

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

// Lamp ID's
int lampIDs[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

int main() {
    // Setup connection
    modbus_t* ctx;
    ctx = modbus_new_tcp("192.168.56.1", 502);
    if (ctx == NULL) {
        fprintf(stderr, "Failed to create Modbus context: %s\n", modbus_strerror(errno));
        errorHandling(lampIDs);
        return -1;
    }

    if (modbus_connect(ctx) == -1) {
        fprintf(stderr, "Connection failed: %s\n", modbus_strerror(errno));
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
    }

    for (;;) {
        int activeMinutes = calcTime(startTime);

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
        }

        if (prevActMinutes != activeMinutes) {
            prevActMinutes = activeMinutes;
        }

        //Sending & retrieveing data to and from the server
        for (int i = 0; i < 5; ++i) {
            // Prepare data to send
            uint16_t dataTx[4] = { level[i], capaciteit[i], energieverbr[i], branduren[i] };

            // Send data
            if (sendData(dataTx, addressTx[i], ctx) == 0) {
                printf("Data sent successfully zone: %d\n", (i + 1));
            }
            else {
                errorHandling(lampIDs);
            }

            // Prepare data to be retrieved
            uint16_t dataRx[2];

            if (retrieveData(dataRx, addressRx[i], ctx) == 0) {
                printf("Data read from Modbus registers zone: %d\n", (i + 1));
                level[i] = dataRx[0]; //setstand[i] = dataRx[0];
                setauto[i] = dataRx[1];
            }
            else {
                errorHandling(lampIDs);
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
        fprintf(stderr, "Write error: %s\n", modbus_strerror(errno));
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
        fprintf(stderr, "Read error: %s\n", modbus_strerror(errno));
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
        printf("Data read from Modbus registers:\n");
        for (int i = 0; i < 2; ++i) { // Changed the loop limit to 2
            printf("Register %d: %d\n", i, dataRx[i]); // Corrected accessing the array directly
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
    int elapsedMinutes = (int)(currentTime - startTime) / 60;
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
    fprintf(stderr, "FATAL ERROR\n");
    for (int i = 0; i < 5; ++i) {
        toggleLamp(1, lampIDs[i]);
        toggleLamp(1, lampIDs[i + 1]);
        setLevel(100, lampIDs[i]);
        setLevel(100, lampIDs[i + 1]);
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
