/*
 * hk_adc.c
 *
 *  Created on: 05-Mar-2021
 *      Author: Sonal Shrivastava
 */

#include "../firmware/drivers/mss_gpio/mss_gpio.h"
#include "../firmware/drivers/mss_i2c/mss_i2c.h"
#include "hk_adc.h"
//#include "stdbool.h"


#define TX_LENGTH           1u
#define RX_LENGTH           2u
#define DUMMY_SERIAL_ADDR   0xFF
#define VBAT_TO_VOUT_RATIO  8u
#define VOUT_TO_IBAT_RATIO  2.5
#define VOLT_TO_MILLIVOLT_RATIO  1000u


/* Variables */
static mss_i2c_status_t status;
static uint16_t read_value;
static uint8_t os_bit = 0;
static uint8_t  rx_buffer[RX_LENGTH];
static float multiplier_to_millivolts = 1.0F;
static int fsr_setting = HK_ADC_CONFIG_FSR_2;
//static bool conv_ready = false;


/* Function prototypes */
static void hk_adc_update_multiplier_volt(void);
static void hk_adc_set_fsr(uint8_t fsr);
static int hk_adc_reg_read(hk_adc_register_t reg, uint16_t *read_buffer);
static int hk_adc_reg_write(hk_adc_register_t reg, uint8_t *write_buffer);



//int hk_adc_initialisation(void)
//{
////    uint16_t result = 0;
//    uint8_t err = HK_ADC_ERR_INIT_FAILED;
////    uint8_t send_buffer[3] = {HK_ADC_REG_CONFIG, 0xE3, 0x03}; //config register, MSB, LSB (Reading CITI TEMP)
////    uint8_t send_buffer[2] = {0xE3, 0x03}; //MSB, LSB (Reading CITI TEMP)
//    uint8_t send_buffer[2] = {HK_ADC_CONFIG_OS_NO_EFFECT | HK_ADC_CONFIG_MUX_SINGLE_2 | fsr_setting | HK_ADC_CONFIG_MODE_SINGLE_SHOT,
//                                HK_ADC_CONFIG_DR_128SPS | HK_ADC_CONFIG_CMODE_TRAD | HK_ADC_CONFIG_CPOL_ACTIVE_LOW | HK_ADC_CONFIG_CLAT_NON_LATCH | HK_ADC_CONFIG_CQUE_DISABLE};
//
//    // Set the clock
//    MSS_I2C_init(&g_mss_i2c0, DUMMY_SERIAL_ADDR, MSS_I2C_PCLK_DIV_256); //10^8/256 = 390,625Hz =~ 390KHz
//
////    //Already in fast mode no special action is required
////    // Write data to slave.
////    MSS_I2C_write(&g_mss_i2c0, HK_ADC_SLAVE_ADDR, &send_buffer[0], sizeof(send_buffer)/sizeof(send_buffer[0]), MSS_I2C_RELEASE_BUS);
////
////    // Wait for completion and record the outcome
////    status = MSS_I2C_wait_complete(&g_mss_i2c0, MSS_I2C_NO_TIMEOUT);
//
//    err = hk_adc_reg_write(HK_ADC_REG_CONFIG, &send_buffer[0]);
//
//    if (err == HK_ADC_NO_ERR)
//    {
////        tx_buffer[0] = HK_ADC_REG_CONVERSION;
////        // Write to Address Pointer Register - pointing to 'reg'
////        MSS_I2C_write(&g_mss_i2c0, HK_ADC_SLAVE_ADDR, &tx_buffer[0], sizeof(tx_buffer)/sizeof(tx_buffer[0]), MSS_I2C_RELEASE_BUS);
////
////        // Wait for completion and record the outcome
////        status = MSS_I2C_wait_complete(&g_mss_i2c0, MSS_I2C_NO_TIMEOUT);
////
////        // Proceed only if it is a success
////        if (status == MSS_I2C_SUCCESS)
////        {
////            // Read data from target slave using MSS I2C 0 (assuming we always read 2B)
////            MSS_I2C_read(&g_mss_i2c0, HK_ADC_SLAVE_ADDR, &rx_buffer[0], read_length, MSS_I2C_RELEASE_BUS);
////
////            status = MSS_I2C_wait_complete(&g_mss_i2c0, MSS_I2C_NO_TIMEOUT);
////
////            if(status == MSS_I2C_SUCCESS)
////            {
////                result = rx_buffer[1] << 8 | rx_buffer[0];
////                return HK_ADC_NO_ERR;
////            }
////        }
//        err = hk_adc_reg_read(HK_ADC_REG_CONVERSION, &read_value);
//        if (err == HK_ADC_NO_ERR)
//        {
//            return err;
//        }
//    }
//
//    return HK_ADC_ERR_INIT_FAILED;
//}

//void hk_adc_start_conv(void)
//{
//    uint8_t err = 0;
//    uint8_t send_buffer[2] = {0};
//    uint16_t tmp = 0;
//
//    err = hk_adc_reg_read(HK_ADC_REG_CONFIG, &read_value);
//    tmp = read_value;
//    if (err == HK_ADC_NO_ERR)
//    {
//        //Write OS bit (MSB) in Config register = 1
//        do{
//            tmp = tmp >> 15;
//        }while(tmp == 0);
//        read_value = OS_BIT_MASK | read_value;
//        send_buffer[0] = read_value >> 8;   //MSB
//        send_buffer[1] = read_value & 0xff; //LSB
//        err = hk_adc_reg_write(HK_ADC_REG_CONFIG, &send_buffer[0]);
//        if (err == HK_ADC_NO_ERR)
//        {
//            err = HK_ADC_NO_ERR;
//        }
//    }
//}



int hk_adc_init(void)
{
    uint8_t err = HK_ADC_ERR_INIT_FAILED;

    // set the full scale voltage range to -4.096 <= V <= +4.096 to support measurement of all ADC inputs
//    hk_adc_set_fsr(HK_ADC_CONFIG_FSR_2);
    fsr_setting = HK_ADC_CONFIG_FSR_2;

    // By default, ADC is configured to point towards CITI_TEMP as MUX input
    //MSB, LSB (Reading CITI TEMP) 0x63 = OS-bit = 0 - no conversion yet
    uint16_t config = HK_ADC_CONFIG_MUX_SINGLE_2 | HK_ADC_CONFIG_FSR_2 | HK_ADC_CONFIG_MODE_SINGLE_SHOT | HK_ADC_CONFIG_DR_128SPS | HK_ADC_CONFIG_CMODE_TRAD | HK_ADC_CONFIG_CPOL_ACTIVE_LOW | HK_ADC_CONFIG_CLAT_NON_LATCH | HK_ADC_CONFIG_CQUE_DISABLE;
    uint8_t send_buffer[2] = {config>>8, config};

//    // initialize gpio
//    MSS_GPIO_init();
//
//    /* Configure MSS GPIO8 in INTERRUPT_MODE (falling edge/active low) since COM_POL is set ACTIVE_LOW*/
//    MSS_GPIO_config(MSS_GPIO_8, MSS_GPIO_INPUT_MODE | MSS_GPIO_IRQ_LEVEL_LOW);
//
//    /* Enable interrupt for GPIO8 */
//    MSS_GPIO_enable_irq(MSS_GPIO_8);

    // Set the clock for i2c0 instance
    MSS_I2C_init(&g_mss_i2c0, DUMMY_SERIAL_ADDR, MSS_I2C_PCLK_DIV_256); //10^8/256 = 390,625Hz =~ 390KHz

    // Write to the CONFIG register
    err = hk_adc_reg_write(HK_ADC_REG_CONFIG, &send_buffer[0]);

    if (err == HK_ADC_NO_ERR)
    {
//        /* set the ALRT/RDY pin in conversion ready mode */
//        // set the MSB of LOW THRESHOLD REG to 0
//        send_buffer[0] = HK_ADC_REG_LOW_THRESH_MSB;      //MSB
//        send_buffer[1] = HK_ADC_REG_LOW_THRESH_LSB;      //LSB
//        err = hk_adc_reg_write(HK_ADC_REG_LO_THRESH, &send_buffer[0]);
//
//        // set the MSB of HIGH THRESHOLD REG to 1
//        send_buffer[0] = HK_ADC_REG_HI_THRESH_MSB;      //MSB
//        send_buffer[1] = HK_ADC_REG_HI_THRESH_LSB;      //LSB
//        err = hk_adc_reg_write(HK_ADC_REG_HI_THRESH, &send_buffer[0]);

        if (err == HK_ADC_NO_ERR)
        {
            return err;
        }
    }

    return err;
}




//void GPIO8_IRQHandler(void)
//{
//    conv_ready = true;
//    MSS_GPIO_clear_irq(MSS_GPIO_8);
//}




int hk_adc_conv_read_volt(float * batt_volt)
{
    uint8_t err = HK_ADC_ERR_VOLT_READ_FAILED;

    uint8_t send_buffer[2] = {0};

    err = hk_adc_reg_read(HK_ADC_REG_CONFIG, &read_value);

    if (err == HK_ADC_NO_ERR)
    {
        // let the rest of the configurations remain intact, just change MUX input config and start single-shot conversion
        read_value = read_value & 0x8fff;
        read_value = read_value | HK_ADC_CONFIG_MUX_SINGLE_1;

        send_buffer[0] = (read_value >> 8) | HK_ADC_CONFIG_OS_SINGLE_CONV;
        send_buffer[1] = read_value;

        // Write changed MUX input back to the CONFIG register
        err = hk_adc_reg_write(HK_ADC_REG_CONFIG, &send_buffer[0]);

        if (err == HK_ADC_NO_ERR)
        {
            //INSERT DELAY FN (1sec), do not use WHILE Loops
            //Wait until OS bit turns 1
            do{
                hk_adc_reg_read(HK_ADC_REG_CONFIG, &read_value);
                os_bit = read_value >> 15;
            }while(os_bit == HK_ADC_CONFIG_OS_NOT_READY);

//            while(conv_ready == false)
//            {
//                // wait here until it turns true
//            }

            err = hk_adc_reg_read(HK_ADC_REG_CONVERSION, &read_value);
            if (err == HK_ADC_NO_ERR)
            {
                // Right shift the received result by 4 bits. Conversion value are 12 most significant bits
                read_value = read_value >> 4;
                // make sure we have the most recent multiplier settings based on the currently configured PGA
                hk_adc_update_multiplier_volt();
                *batt_volt = ((float)read_value * multiplier_to_millivolts) * VBAT_TO_VOUT_RATIO/VOLT_TO_MILLIVOLT_RATIO;

//                // rest the conv_ready flag to false
//                conv_ready = false;
                return err;
            }
        }
    }
    return err;
}




int hk_adc_conv_read_curr(float * batt_curr)
{
    uint8_t err = HK_ADC_ERR_CURR_READ_FAILED;

    uint8_t send_buffer[2] = {0};

    err = hk_adc_reg_read(HK_ADC_REG_CONFIG, &read_value);

    if (err == HK_ADC_NO_ERR)
    {
        // let the rest of the configurations remain intact, just change MUX input config and start single-shot conversion
        read_value = read_value & 0x8fff;
        read_value = read_value | HK_ADC_CONFIG_MUX_SINGLE_0;

        send_buffer[0] = (read_value >> 8) | HK_ADC_CONFIG_OS_SINGLE_CONV;
        send_buffer[1] = read_value;

        // Write changed MUX input back to the CONFIG register
        err = hk_adc_reg_write(HK_ADC_REG_CONFIG, &send_buffer[0]);

        if (err == HK_ADC_NO_ERR)
        {
            //INSERT DELAY FN (1sec), do not use WHILE Loops
            //Wait until OS bit turns 1
            do{
                hk_adc_reg_read(HK_ADC_REG_CONFIG, &read_value);
                os_bit = read_value >> 15;
            }while(os_bit == HK_ADC_CONFIG_OS_NOT_READY);

            err = hk_adc_reg_read(HK_ADC_REG_CONVERSION, &read_value);
            if (err == HK_ADC_NO_ERR)
            {
                // Right shift the received result by 4 bits. Conversion value are 12 most significant bits
                read_value = read_value >> 4;
                // make sure we have the most recent multiplier settings based on the currently configured PGA
                hk_adc_update_multiplier_volt();
                *batt_curr = (((float)read_value * multiplier_to_millivolts)/(VOUT_TO_IBAT_RATIO * VOLT_TO_MILLIVOLT_RATIO));     // Current in Amps;
                return err;
            }
        }
    }
    return err;
}



int hk_adc_conv_read_citi_temp(float * citi_temp)
{
    uint8_t err = HK_ADC_ERR_TEMP_READ_FAILED;

    uint8_t send_buffer[2] = {0};

    err = hk_adc_reg_read(HK_ADC_REG_CONFIG, &read_value);

    if (err == HK_ADC_NO_ERR)
    {
        // let the rest of the configurations remain intact, just change MUX input config and start single-shot conversion
        read_value = read_value & 0x8fff;
        read_value = read_value | HK_ADC_CONFIG_MUX_SINGLE_2;

        send_buffer[0] = (read_value >> 8) | HK_ADC_CONFIG_OS_SINGLE_CONV;
        send_buffer[1] = read_value;

        // Write changed MUX input back to the CONFIG register
        err = hk_adc_reg_write(HK_ADC_REG_CONFIG, &send_buffer[0]);

        if (err == HK_ADC_NO_ERR)
        {
            //INSERT DELAY FN (1sec), do not use WHILE Loops
            //Wait until OS bit turns 1
            do{
                hk_adc_reg_read(HK_ADC_REG_CONFIG, &read_value);
                os_bit = read_value >> 15;
            }while(os_bit == HK_ADC_CONFIG_OS_NOT_READY);

            err = hk_adc_reg_read(HK_ADC_REG_CONVERSION, &read_value);
            if (err == HK_ADC_NO_ERR)
            {
                // Right shift the received result by 4 bits. Conversion value are 12 most significant bits
                read_value = read_value >> 4;
                // make sure we have the most recent multiplier settings based on the currently configured PGA
                hk_adc_update_multiplier_volt();
                *citi_temp = ((float)read_value * multiplier_to_millivolts/VOLT_TO_MILLIVOLT_RATIO);    //result passed onto CITIROC UI should be in Volts (Vtemp)
                return err;
            }
        }
    }
    return err;
}




/*
int hk_adc_reg_read_conv(uint16_t *read_buffer)
{
//    uint16_t result = 0;
//    uint8_t send_buffer = HK_ADC_REG_CONVERSION;
    uint8_t err = HK_ADC_ERR_READ_FAILED;
    uint8_t read_os_bit = 0;

//    // Write to Address Pointer Register - pointing to 'reg'
//    MSS_I2C_write(&g_mss_i2c0, HK_ADC_SLAVE_ADDR, &send_buffer, TX_LENGTH, MSS_I2C_RELEASE_BUS);
//
//    // Wait for completion and record the outcome
//    status = MSS_I2C_wait_complete(&g_mss_i2c0, MSS_I2C_NO_TIMEOUT);
//
//    // Proceed only if it is a success
//    if (status == MSS_I2C_SUCCESS)
//    {
//        // Read data from target slave (we always read 2B)
//        MSS_I2C_read(&g_mss_i2c0, HK_ADC_SLAVE_ADDR, &rx_buffer[0], RX_LENGTH, MSS_I2C_RELEASE_BUS);
//
//        status = MSS_I2C_wait_complete(&g_mss_i2c0, MSS_I2C_NO_TIMEOUT);
//
//        if(status == MSS_I2C_SUCCESS)
//        {
//            // Right shift the read data by 4bits since conversion reg holds 12-bit result D[15:4]
//            result = (rx_buffer[0] << 8 | rx_buffer[1]) >> 4;
//            *read_buffer = result;
//            return HK_ADC_NO_ERR;
//        }
//    }

    // OR

    do{
        hk_adc_reg_read(HK_ADC_REG_CONFIG, &read_value);
        read_os_bit = read_value >> 15;
    }while(read_os_bit == 0);

    err = hk_adc_reg_read(HK_ADC_REG_CONVERSION, &read_value);
    if (err == HK_ADC_NO_ERR)
    {
        // Right shift the received result by 4 bits. Conversion value are 12 most significant bits
        read_value = read_value >> 4;
        *read_buffer = read_value;
        return err;
    }

    return err;
}
*/


int hk_adc_reg_write(hk_adc_register_t reg, uint8_t *write_buffer)
{
    uint8_t send_buffer[3] = {0xff};
    uint8_t err = HK_ADC_ERR_WRITE_FAILED;
//    uint16_t tmp = 0;
//    uint8_t read_os_bit = 0;

    // assign register address pointer
    switch(reg)
    {
        case HK_ADC_REG_CONVERSION:
            send_buffer[0] = HK_ADC_REG_PTR_CONVERSION;
            break;

        case HK_ADC_REG_CONFIG:
            send_buffer[0] = HK_ADC_REG_PTR_CONFIG;
            break;

        case HK_ADC_REG_LO_THRESH:
            send_buffer[0] = HK_ADC_REG_PTR_LOW_THRESH;
            break;

        case HK_ADC_REG_HI_THRESH:
            send_buffer[0] = HK_ADC_REG_PTR_HI_THRESH;
            break;

//        default:
            //no other register possible
    }

    send_buffer[1] = *write_buffer;         //MSB
    send_buffer[2] = *(write_buffer + 1);   //LSB
//    tmp = send_buffer[1]<<8 | send_buffer[2];

    // Write data to slave.
    MSS_I2C_write(&g_mss_i2c0, HK_ADC_ADDRESS_GND, &send_buffer[0], sizeof(send_buffer)/sizeof(send_buffer[0]), MSS_I2C_RELEASE_BUS);

    // Wait for completion and record the outcome
    status = MSS_I2C_wait_complete(&g_mss_i2c0, MSS_I2C_NO_TIMEOUT);

    if (status == MSS_I2C_SUCCESS)
    {
//        do{
//            err = hk_adc_reg_read(reg, &read_value);
//        }while(read_value != tmp);
//        err = hk_adc_reg_read(reg, &read_value);

//        if (err == HK_ADC_NO_ERR)
//        {
//            if (read_value == tmp)
//            {
                err = HK_ADC_NO_ERR;
                return err;
//            }
//        }
    }

    err = HK_ADC_ERR_WRITE_FAILED;
    return err;
}


int hk_adc_reg_read(hk_adc_register_t reg, uint16_t *read_buffer)
{
//    uint16_t result = 0;
    uint8_t send_buffer = 0xff;

    // assign register address pointer
    switch(reg)
    {
        case HK_ADC_REG_CONVERSION:
            send_buffer = HK_ADC_REG_PTR_CONVERSION;
            break;

        case HK_ADC_REG_CONFIG:
            send_buffer = HK_ADC_REG_PTR_CONFIG;
            break;

        case HK_ADC_REG_LO_THRESH:
            send_buffer = HK_ADC_REG_PTR_LOW_THRESH;
            break;

        case HK_ADC_REG_HI_THRESH:
            send_buffer = HK_ADC_REG_PTR_HI_THRESH;
            break;

//        default:
            //no other register possible
    }

    // Write to Address Pointer Register - pointing to 'reg'
    MSS_I2C_write(&g_mss_i2c0, HK_ADC_ADDRESS_GND, &send_buffer, TX_LENGTH, MSS_I2C_RELEASE_BUS);

    // Wait for completion and record the outcome
    status = MSS_I2C_wait_complete(&g_mss_i2c0, MSS_I2C_NO_TIMEOUT);

    // Proceed only if it is a success
    if (status == MSS_I2C_SUCCESS)
    {
        // Read data from target slave (we always read 2B)
        MSS_I2C_read(&g_mss_i2c0, HK_ADC_ADDRESS_GND, &rx_buffer[0], RX_LENGTH, MSS_I2C_RELEASE_BUS);

        status = MSS_I2C_wait_complete(&g_mss_i2c0, MSS_I2C_NO_TIMEOUT);

        if(status == MSS_I2C_SUCCESS)
        {
            read_value = rx_buffer[0] << 8 | rx_buffer[1];
            *read_buffer = read_value;
            return HK_ADC_NO_ERR;
        }
    }

    return HK_ADC_ERR_READ_FAILED;
}




void hk_adc_set_fsr(uint8_t fsr)
{
    fsr_setting = fsr;
    hk_adc_update_multiplier_volt();
}




void hk_adc_update_multiplier_volt(void)
{
    switch(fsr_setting)
    {
        case HK_ADC_CONFIG_FSR_1:
            multiplier_to_millivolts = 3.0F;
            break;

        case HK_ADC_CONFIG_FSR_2:
            multiplier_to_millivolts = 2.0F;
            break;

        case HK_ADC_CONFIG_FSR_3:
            multiplier_to_millivolts = 1.0F;
            break;

        case HK_ADC_CONFIG_FSR_4:
            multiplier_to_millivolts = 0.5F;
            break;

        case HK_ADC_CONFIG_FSR_5:
            multiplier_to_millivolts = 0.25F;
            break;

        case HK_ADC_CONFIG_FSR_6:
            multiplier_to_millivolts = 0.125F;
            break;

        default:
            multiplier_to_millivolts = 1.0F;
    }
}
