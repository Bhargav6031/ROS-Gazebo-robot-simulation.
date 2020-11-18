/* 
 * File:   ICM-20948.c
 * Author: Aaron Hunter
 * Brief: Library for the ICM-20948 IMU
 * Created on Nov 13, 2020 9:46 am
 * Modified on 
 */

/*******************************************************************************
 * #INCLUDES                                                                   *
 ******************************************************************************/

#include "ICM-20948.h" // The header file for this source file. 
#include "SerialM32.h"
#include "Board.h"
#include <stdio.h>
#include <sys/attribs.h>  //for ISR definitions
#include <proc/p32mx795f512l.h>


/*******************************************************************************
 * PRIVATE #DEFINES                                                            *
 ******************************************************************************/
#define IMU_BAUD 400000
#define ICM_I2C_ADDR 0b1101001 //=decimal 69
#define BYPASS_EN 0X2
#define MAG_NUM_BYTES 9
#define IMU_NUM_BYTES 12
#define MAG_MODE_4 0b01000
#define MAG_MODE_3 0b00110
#define MAG_MODE_2 0b00100
#define MAG_MODE_1 0b00010
#define MAG_MODE_0 0b00001
#define MAG_I2C_ADDR 0b0001100  //hex 0C
#define MAG_WHO_I_AM_1 0
#define MAG_WHO_I_AM_2 1
#define ACK 0
#define NACK 1
#define READ 1
#define WRITE 0

/*******************************************************************************
 * PRIVATE TYPEDEFS                                                            *
 ******************************************************************************/
typedef enum {
    IMU_IDLE,
    TX_ADDRESS_SENT,
    TX_REGISTER_SENT,
    TX_SETTINGS_SENT,
    TX_STOP,
    RX_START_SET_REGISTER,
    RX_SEND_ADDRESS,
    RX_SEND_REGISTER,
    RX_STOP_SET_REGISTER,
    RX_START_READ,
    RX_READ_SEND_ADDRESS,
    RX_READ_HIGH_BYTE,
    RX_ACK_HIGH_BYTE,
    RX_READ_LOW_BYTE,
    RX_NACK_LOW_BYTE,
    RX_STOP_READ,
} IMU_config_states_t;

typedef enum {
    // Gyroscope and Accelerometer
    // User Bank 0
    AGB0_REG_WHO_AM_I = 0x00,
    AGB0_REG_USER_CTRL = 0x03,
    AGB0_REG_LP_CONFIG = 0x05,
    AGB0_REG_PWR_MGMT_1,
    AGB0_REG_PWR_MGMT_2,
    AGB0_REG_INT_PIN_CONFIG = 0x0F,
    AGB0_REG_INT_ENABLE,
    AGB0_REG_INT_ENABLE_1,
    AGB0_REG_INT_ENABLE_2,
    AGB0_REG_INT_ENABLE_3,
    AGB0_REG_I2C_MST_STATUS = 0x17,
    AGB0_REG_INT_STATUS = 0x19,
    AGB0_REG_INT_STATUS_1,
    AGB0_REG_INT_STATUS_2,
    AGB0_REG_INT_STATUS_3,
    AGB0_REG_DELAY_TIMEH = 0x28,
    AGB0_REG_DELAY_TIMEL,
    AGB0_REG_ACCEL_XOUT_H = 0x2D,
    AGB0_REG_ACCEL_XOUT_L,
    AGB0_REG_ACCEL_YOUT_H,
    AGB0_REG_ACCEL_YOUT_L,
    AGB0_REG_ACCEL_ZOUT_H,
    AGB0_REG_ACCEL_ZOUT_L,
    AGB0_REG_GYRO_XOUT_H,
    AGB0_REG_GYRO_XOUT_L,
    AGB0_REG_GYRO_YOUT_H,
    AGB0_REG_GYRO_YOUT_L,
    AGB0_REG_GYRO_ZOUT_H,
    AGB0_REG_GYRO_ZOUT_L,
    AGB0_REG_TEMP_OUT_H,
    AGB0_REG_TEMP_OUT_L,
    AGB0_REG_EXT_SLV_SENS_DATA_00,
    AGB0_REG_EXT_SLV_SENS_DATA_01,
    AGB0_REG_EXT_SLV_SENS_DATA_02,
    AGB0_REG_EXT_SLV_SENS_DATA_03,
    AGB0_REG_EXT_SLV_SENS_DATA_04,
    AGB0_REG_EXT_SLV_SENS_DATA_05,
    AGB0_REG_EXT_SLV_SENS_DATA_06,
    AGB0_REG_EXT_SLV_SENS_DATA_07,
    AGB0_REG_EXT_SLV_SENS_DATA_08,
    AGB0_REG_EXT_SLV_SENS_DATA_09,
    AGB0_REG_EXT_SLV_SENS_DATA_10,
    AGB0_REG_EXT_SLV_SENS_DATA_11,
    AGB0_REG_EXT_SLV_SENS_DATA_12,
    AGB0_REG_EXT_SLV_SENS_DATA_13,
    AGB0_REG_EXT_SLV_SENS_DATA_14,
    AGB0_REG_EXT_SLV_SENS_DATA_15,
    AGB0_REG_EXT_SLV_SENS_DATA_16,
    AGB0_REG_EXT_SLV_SENS_DATA_17,
    AGB0_REG_EXT_SLV_SENS_DATA_18,
    AGB0_REG_EXT_SLV_SENS_DATA_19,
    AGB0_REG_EXT_SLV_SENS_DATA_20,
    AGB0_REG_EXT_SLV_SENS_DATA_21,
    AGB0_REG_EXT_SLV_SENS_DATA_22,
    AGB0_REG_EXT_SLV_SENS_DATA_23,
    AGB0_REG_FIFO_EN_1 = 0x66,
    AGB0_REG_FIFO_EN_2,
    AGB0_REG_FIFO_MODE,
    AGB0_REG_FIFO_COUNT_H = 0x70,
    AGB0_REG_FIFO_COUNT_L,
    AGB0_REG_FIFO_R_W,
    AGB0_REG_DATA_RDY_STATUS = 0x74,
    AGB0_REG_FIFO_CFG = 0x76,
    AGB0_REG_MEM_START_ADDR = 0x7C, // Hmm, Invensense thought they were sneaky not listing these locations on the datasheet...
    AGB0_REG_MEM_R_W = 0x7D, // These three locations seem to be able to access some memory within the device
    AGB0_REG_MEM_BANK_SEL = 0x7E, // And that location is also where the DMP image gets loaded
    AGB0_REG_REG_BANK_SEL = 0x7F,
} ICM_USR_Bank_0_e;

typedef enum {
    WIA1 = 0x00,
    WIA2,
    ST1 = 0x10, //status 1 shows when data is ready and if data has been skipped
    HXL, //data registers
    HXH,
    HYL,
    HYH,
    HZL,
    HZH,
    TMPS, //dummy register    
    ST2, //status 2, bit 3 indicates mag sensor overflow, must read to allow new data collection
    CNTL2 = 0x31, //control 2--sets the mode of operation
    CNTL3,
} Mag_reg_e;

static int8_t I2C_is_configured = FALSE;

static uint8_t Mag_data[MAG_NUM_BYTES];
static uint8_t IMU_data[IMU_NUM_BYTES];
/*******************************************************************************
 * PRIVATE FUNCTIONS PROTOTYPES                                                 *
 ******************************************************************************/
/*blocking functions, deprecated*/
void I2C_start(void);
void I2C_stop(void);
void I2C_restart(void);
int I2C_sendByte(unsigned char byte);
unsigned char I2C_readByte(void);
uint8_t IMU_get_byte(uint8_t i2c_addr, uint8_t reg_addr);
uint8_t ICM_set_reg(uint8_t i2c_addr, uint8_t reg_addr, uint8_t setting);
void MAG_read_data();
/***********************************/
void IMU_cfg_I2C_state_machine(void);
/*******************************************************************************
 * PUBLIC FUNCTION IMPLEMENTATIONS                                             *
 ******************************************************************************/

/**
 * @Function IMU_init(void)
 * @return SUCCESS or ERROR
 * @brief initializes the I2C system for IMU operation
 * @note 
 * @author Aaron Hunter,
 * @modified  */
uint8_t IMU_init(void) {
    uint32_t pb_clk;
    __builtin_disable_interrupts();
    /*config LIDAR I2C interrupt*/
    /*config priority and subpriority--must match IPL level*/
    IPC6bits.I2C1IP = 2;
    IPC6bits.I2C1IS = 0;
    /*enable master interrupt*/
    //    IEC0bits.I2C1MIE = 1;
    /*clear master interrupt flag*/
    IFS0bits.I2C1MIF = 0;
    /*restart interrupts*/
    __builtin_enable_interrupts();

    I2C1CON = 0; //reset config; turn off I2C
    /*calculate the BRG from the baud rate*/
    /*FSCK = (PBCLK) / ((I2CxBRG+2) * 2)
     * I2CBRG = (PBCLK / (2 *FSCK)) - 2*/
    pb_clk = Board_get_PB_clock();
    I2C1BRG = (pb_clk / (2 * IMU_BAUD)) - 2; //set baud rate to 400KHz-->verify with Oscope for correct timing
    //    printf("Board set to BRG %d",I2C1BRG);
    I2C1CONbits.ON = 1; //enable I2C
    printf("I2C1 CON state %X", I2C1CON);
    /* send start command to write settings to device */
    //    I2C1CONbits.SEN = 1;
    /* Note 1: When using the 1:1 PBCLK divisor, the user?s software should not
     * read/write the peripheral?s SFRs in the SYSCLK cycle immediately
     * following the instruction that clears the module?s ON bit.*/

    return SUCCESS;
}

/*******************************************************************************
 * PRIVATE FUNCTION IMPLEMENTATIONS                                            *
 ******************************************************************************/
void I2C_start(void) {
    I2C1CONbits.SEN = 1;
    while (I2C1CONbits.SEN) {
        ;
    } //wait for start bit to be cleared
}

void I2C_stop(void) {
    I2C1CONbits.PEN = 1; //send stop condition
    while (I2C1CONbits.PEN) {
        ;
    } //wait for stop bit to be cleared
}

void I2C_restart(void) {
    I2C1CONbits.RSEN = 1;
    while (I2C1CONbits.RSEN) {
        ;
    } //wait for restart bit to be cleared
}

int I2C_sendByte(unsigned char byte) {
    I2C1TRN = byte; //if an address bit0 = 0 for write, 1 for read
    while (I2C1STATbits.TRSTAT) { //wait for byte to be transmitted
        ;
    }
    if (I2C1STATbits.ACKSTAT == 1) { //not acknowledged, transmission failed
        return ERROR;
    }
    return SUCCESS;
}

uint8_t IMU_get_byte(uint8_t i2c_addr, uint8_t reg_addr) {
    uint8_t ret_val;
    uint8_t err_state;
    I2C_start();
    I2C_sendByte(i2c_addr << 1 | WRITE); //read from device
    I2C_sendByte(reg_addr);
    // I2C_stop();
    I2C_restart();
    err_state = I2C_sendByte(i2c_addr << 1 | READ);
    ret_val = I2C_readByte();
    I2C_stop();
    return ret_val;
}

uint8_t ICM_set_reg(uint8_t i2c_addr, uint8_t reg_addr, uint8_t setting) {
    uint8_t ret_val;
    uint8_t err_state;
    I2C_start();
    I2C_sendByte(i2c_addr << 1 | WRITE); //read from device
    I2C_sendByte(reg_addr);
    I2C_sendByte(setting);
    I2C_stop();
    return SUCCESS;
}

unsigned char I2C_readByte(void) {
    I2C1CONbits.RCEN = 1; //setting this bit initiates a receive.  Hardware clears after the received has finished
    while (I2C1CONbits.RCEN) { //wait for byte to be received
        ;
    }
    return I2C1RCV; //return the value in receive buffer
}

void __ISR(_I2C1_VECTOR, IPL2AUTO) IMU_interrupt_handler(void) {
    IFS0bits.I2C1MIF = 0; //clear flag
    IMU_cfg_I2C_state_machine();
    LATEINV = 0x4; //toggle led3 RE2
}

void IMU_read_data() {
    uint8_t i;
    uint8_t err_state;
    uint8_t num_bytes = IMU_NUM_BYTES; // TODO fix this for final version to reflect whole array
    I2C_start();
    I2C_sendByte(ICM_I2C_ADDR << 1 | WRITE);
    I2C_sendByte(AGB0_REG_ACCEL_XOUT_H);
    //    I2C_stop();
    I2C_restart();
    I2C_sendByte(ICM_I2C_ADDR << 1 | READ); //read from device

    for (i = 0; i < num_bytes; i++) {
        I2C1CONbits.RCEN = 1; //setting this bit initiates a receive.  Hardware clears after the received has finished
        while (I2C1CONbits.RCEN) { //wait for byte to be received
            ;
        }
        IMU_data[i] = I2C1RCV;
        /*ACK the byte except last one*/
        if (i < (num_bytes - 1)) {
            I2C1CONbits.ACKDT = 0; // NACK the byte
            I2C1CONbits.ACKEN = 1;
            while (I2C1CONbits.ACKEN == 1);
        }
    }
    I2C_stop();
}

void MAG_read_data() {
    uint8_t i;
    uint8_t err_state;
    uint8_t num_bytes = MAG_NUM_BYTES; // TODO fix this for final version to reflect whole array
    I2C_start();
    I2C_sendByte(MAG_I2C_ADDR << 1 | WRITE);
    I2C_sendByte(ST1);
    I2C_restart();
    err_state = I2C_sendByte(MAG_I2C_ADDR << 1 | READ);

    for (i = 0; i < num_bytes; i++) {
        I2C1CONbits.RCEN = 1; //setting this bit initiates a receive.  Hardware clears after the received has finished
        while (I2C1CONbits.RCEN) { //wait for byte to be received
            ;
        }
        Mag_data[i] = I2C1RCV;
        /*ACK the byte except last one*/
        if (i < (num_bytes - 1)) {
            I2C1CONbits.ACKDT = 0; // NACK the byte
            I2C1CONbits.ACKEN = 1;
            while (I2C1CONbits.ACKEN == 1);
        }
    }
    I2C_stop();
}

void IMU_cfg_I2C_state_machine(void) {
    static IMU_config_states_t current_state = IMU_IDLE;
    static IMU_config_states_t next_state = IMU_IDLE;
    static uint8_t error = FALSE;

    switch (current_state) {
        case(IMU_IDLE):
            next_state = IMU_IDLE;
            break;
        default:
            break;
    }
    current_state = next_state;
}



/**
 * @Function someFunction(void)
 * @param foo, some value
 * @return TRUE or FALSE
 * @brief 
 * @note 
 * @author <Your Name>
 * @modified <Your Name>, <year>.<month>.<day> <hour> <pm/am> */
uint8_t someFunction(int foo);

#ifdef ICM_TESTING

int main(void) {
    int value = 0;
    int i;
    int16_t acc_x;
    int16_t acc_y;
    int16_t acc_z;
    int16_t gyr_x;
    int16_t gyr_y;
    int16_t gyr_z;

    int16_t mag_x;
    int16_t mag_y;
    int16_t mag_z;

    uint8_t mag_status_1;
    uint8_t mag_status_2;

    int errstate = 0;
    Board_init();
    Serial_init();
    IMU_init();
    printf("\r\nICM-20948 Test Harness %s, %s\r\n", __DATE__, __TIME__);
    value = IMU_get_byte(ICM_I2C_ADDR, AGB0_REG_WHO_AM_I);
    printf("ICM returned %d \r\n", value);
    /*turn on device by clearing bit 6 and setting bit1*/
    ICM_set_reg(ICM_I2C_ADDR, AGB0_REG_PWR_MGMT_1, 0x2);
    value = IMU_get_byte(ICM_I2C_ADDR, AGB0_REG_PWR_MGMT_1);
    printf("ICM returned %d \r\n", value);
    /*attempt to use I2C bypass feature*/
    ICM_set_reg(ICM_I2C_ADDR, AGB0_REG_INT_PIN_CONFIG, BYPASS_EN);
    value = IMU_get_byte(ICM_I2C_ADDR, AGB0_REG_INT_PIN_CONFIG);
    printf("ICM returned %d \r\n", value);
    /*now see if we can contact the mag*/
    value = IMU_get_byte(MAG_I2C_ADDR, MAG_WHO_I_AM_1);
    printf("MAG returned 0x%x \r\n", value);
    ICM_set_reg(MAG_I2C_ADDR, CNTL2, MAG_MODE_3);
    value = IMU_get_byte(MAG_I2C_ADDR, CNTL2);
    printf("MAG returned 0x%x \r\n", value);

    while (1) {
        IMU_read_data();
        acc_x = IMU_data[0] << 8 | IMU_data[1];
        acc_y = IMU_data[2] << 8 | IMU_data[3];
        acc_z = IMU_data[4] << 8 | IMU_data[5];
        gyr_x = IMU_data[6] << 8 | IMU_data[7];
        gyr_y = IMU_data[8] << 8 | IMU_data[9];
        gyr_z = IMU_data[10] << 8 | IMU_data[11];
        MAG_read_data();
        mag_status_1 = Mag_data[0] & 0x3;
        mag_x = Mag_data[2] << 8 | Mag_data[1];
        mag_y = Mag_data[4] << 8 | Mag_data[3];
        mag_z = Mag_data[6] << 8 | Mag_data[5];
        mag_status_2 = Mag_data[8] & 0x4;
        printf("a:[%d, %d, %d], g:[%d, %d, %d], m:[%d, %d, %d]\r\n", acc_x, acc_y, acc_z,gyr_x, gyr_y, gyr_z, mag_x, mag_y, mag_z);
//        printf("MAG: DR/ODR: %d, %d, %d, %d, HOFL: %x \r\n", mag_status_1, mag_x, mag_y, mag_z, mag_status_2);
        for (i = 0; i < 1000000; i++) {
            ;
        }
    }
}
#endif //ICM_TESTING

