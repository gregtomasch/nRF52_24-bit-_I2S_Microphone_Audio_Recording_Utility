/* Copyright (c) 2015 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 *
 */

/** @file
 * @defgroup i2s_example_main main.c
 * @{
 * @ingroup i2s_example
 *
 * @brief I2S Audio Capture Application main file.
 *
 * This file contains the source code for a sample application using I2S.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "app_uart.h"
#include "nrf.h"
#include "bsp.h"
#include "app_pwm.h"
#include "nrf_drv_i2s.h"
#include "nrf_delay.h"
#include "app_util_platform.h"
#include "app_error.h"
#include "boards.h"

#define PWM_I2S_SCK_PIN     13                                                                             // I2S SCK pin for PWM-synthesized I2S clocks
#define PWM_I2S_WS_PIN      17                                                                             // I2S LRCK pin for PWM-synthesized I2S clocks
#define I2S_BUFFER_SIZE     64                                                                             // Data handler is called when I2S data bufffer contains (I2S_BUFFER_SIZE/2) 32bit words
#define UART_TX_BUF_SIZE    256                                                                            // UART TX FIFO buffer size in bytes
#define UART_RX_BUF_SIZE    256                                                                            // UART RX FIFO buffer size in bytes
#define RINGBUFFER_SIZE     8192                                                                           // Size in bytes of ringbuffer between I2S and UART

static uint32_t          m_buffer_rx[I2S_BUFFER_SIZE];
static uint32_t          lsample_buffer_rx[(I2S_BUFFER_SIZE/2)];
static volatile uint8_t  lsample_byte_buffer_rx[(I2S_BUFFER_SIZE*2)];
static volatile uint8_t  ringbuffer[RINGBUFFER_SIZE];
static volatile uint32_t i2s_write = 0;                                                                    // Ring buffer write pointer
static volatile uint32_t i2s_read  = 0;                                                                    // Ring buffer read pointer
static bool              m_error_encountered;                                                              // I2S data callback error status
static volatile bool     ready_flag;                                                                       // A PWM ready status
union
{
  uint32_t word;
  uint8_t  byte_decomp[4];
}u;


void uart_error_handle(app_uart_evt_t * p_event)                                                           // UART error handler
{
    if (p_event->evt_type == APP_UART_COMMUNICATION_ERROR)
    {
        APP_ERROR_HANDLER(p_event->data.error_communication);
    }
    else if (p_event->evt_type == APP_UART_FIFO_ERROR)
    {
        APP_ERROR_HANDLER(p_event->data.error_code);
    }
}


void pwm_ready_callback(uint32_t pwm_id)                                                                   // PWM ready callback function
{
	ready_flag = true;
}


static bool copy_samples(uint32_t const * p_buffer, uint16_t number_of_words)                              // I2S data callback read and ring buffer write function
{
	uint32_t count, size, offset = 0;
	
	memcpy(lsample_buffer_rx, p_buffer, (4*number_of_words));                                              // Copy I2S data from the callback buffer to a volitile buffer
	
	// Parse I2S callback data and load into a byte array
	for(uint32_t i=0; i<number_of_words; i++)
	{
	  u.word = lsample_buffer_rx[i];
	  for(uint8_t j=0; j<4; j++)
	  {
	    lsample_byte_buffer_rx[((4*i) + j)] = u.byte_decomp[3-j];
	  }
	}
	
	// Load I2S byte array into the ring buffer and index write pointer
	count = (4*number_of_words);
	size = count;
	if((i2s_write + count) > RINGBUFFER_SIZE)                                                              // IF I2S byte count is larger than the space before pointer wrap, break into two pieces
    {
      count = RINGBUFFER_SIZE - i2s_write;
    }
    if(count)
	{
	  for(uint32_t i=0; i<count; i++)
	  {
	    ringbuffer[i2s_write + i] = lsample_byte_buffer_rx[i];
	  }
	  i2s_write += count;
      size -= count;
      offset = count;
      if(i2s_write == RINGBUFFER_SIZE)                                                                     // Wrap the write pointer
      {
        i2s_write = 0;
      }
    }
    count = size;
    if(count)                                                                                              // If broken into two parts, do the second part
	{
	  for(uint32_t i=0; i<count; i++)
	  {
	     ringbuffer[i2s_write + i] = lsample_byte_buffer_rx[offset + i];
	  }
	  i2s_write += count;
      if(i2s_write == RINGBUFFER_SIZE)
      {
        i2s_write = 0;
      }
    }
	  
    return true;
}


static void check_rx_data(uint32_t const * p_buffer, uint16_t number_of_words)
{
    if (!m_error_encountered)
    {
        m_error_encountered = !copy_samples(p_buffer, number_of_words);
    }
}


// This is the I2S data handler - all data exchange related to the I2S transfers is done here.
static void data_handler(uint32_t const * p_data_received,
                         uint32_t       * p_data_to_send,
                         uint16_t         number_of_words)
{
    // Non-NULL value in 'p_data_received' indicates that a new portion of
    // data has been received and should be processed.
    if (p_data_received != NULL)
    {
        check_rx_data(p_data_received, number_of_words);
    }

    // Non-NULL value in 'p_data_to_send' indicates that the driver needs
    // a new portion of data to send. Nothing done here; RX only...
    if (p_data_to_send != NULL)
    {
    }
}


void app_error_fault_handler(uint32_t id, uint32_t pc, uint32_t info)
{
    #ifdef DEBUG
    app_error_print(id, pc, info);
    #endif

    while (1);
}


APP_PWM_INSTANCE(PWM1,0);                                                                                  // Create the instance "PWM1" using TIMER0.
APP_PWM_INSTANCE(PWM2,1);                                                                                  // Create the instance "PWM2" using TIMER1.

int main(void)
{
    uint32_t err_code = NRF_SUCCESS;
	
    const app_uart_comm_params_t comm_params =                                                             // UART port parameter srtucture variable
      {
          RX_PIN_NUMBER,
          TX_PIN_NUMBER,
          RTS_PIN_NUMBER,
          CTS_PIN_NUMBER,
          APP_UART_FLOW_CONTROL_ENABLED,
          false,
          UART_BAUDRATE_BAUDRATE_Baud921600                                                                // Set the baud rate to 11...
      };

    APP_UART_FIFO_INIT(&comm_params,                                                                       // UART port instantiation
                         UART_RX_BUF_SIZE,
                         UART_TX_BUF_SIZE,
                         uart_error_handle,
                         APP_IRQ_PRIORITY_LOWEST,
                         err_code);
    APP_ERROR_CHECK(err_code);
		   
	// Define the I2S configuration; running in slave mode and using PWM outputs to provide synthetic SCK and LRCK
    nrf_drv_i2s_config_t config = NRF_DRV_I2S_DEFAULT_CONFIG;
    config.sdin_pin             = I2S_SDIN_PIN;
    config.sdout_pin            = I2S_SDOUT_PIN;
    config.mode                 = NRF_I2S_MODE_SLAVE;
    config.mck_setup            = NRF_I2S_MCK_DISABLED;
    config.sample_width         = NRF_I2S_SWIDTH_24BIT;
    config.channels             = NRF_I2S_CHANNELS_LEFT;                                                    // Set the I2S microphone to output on the left channel
    config.format               = NRF_I2S_FORMAT_I2S;
    err_code                    = nrf_drv_i2s_init(&config, data_handler);                                  // Initialize the I2S driver
    APP_ERROR_CHECK(err_code);
	
    // 1-channel PWM; 16MHz clock and period set in ticks.
    // The user is responsible for selecting the periods to guve the correct ratio for the I2S frame length
    app_pwm_config_t pwm1_cfg = APP_PWM_DEFAULT_CONFIG_1CH(25L, PWM_I2S_SCK_PIN);                           // SCK; pick a convenient gpio pin
    app_pwm_config_t pwm2_cfg = APP_PWM_DEFAULT_CONFIG_1CH(1600L, PWM_I2S_WS_PIN);                          // LRCK; pick a convenient gpio pin. LRCK period = 64X SCK period

    // Initialize and enable PWM's
    err_code = app_pwm_ticks_init(&PWM1,&pwm1_cfg,pwm_ready_callback);
    APP_ERROR_CHECK(err_code);
    err_code = app_pwm_ticks_init(&PWM2,&pwm2_cfg,pwm_ready_callback);
    APP_ERROR_CHECK(err_code);
    app_pwm_enable(&PWM1);
    app_pwm_enable(&PWM2);
    app_pwm_channel_duty_set(&PWM1, 0, 50);                                                                // Set at 50% duty cycle for square wave
    app_pwm_channel_duty_set(&PWM2, 0, 50);

    // Instantiate I2S
    err_code = nrf_drv_i2s_start(m_buffer_rx, NULL,                                                        // RX only; "NULL" XMIT buffer
       I2S_BUFFER_SIZE, 0);
    APP_ERROR_CHECK(err_code);
    memset(m_buffer_rx, 0xCC, sizeof(m_buffer_rx));                                                        // Initialize I2S data callback buffer
	
    for (;;)
    {
      uint32_t count = 0;
      uint32_t size = 0;

      // Push data from the ring buffer to the UART
      while(i2s_read != i2s_write)
      {
        if(i2s_write > i2s_read)
        {
          count = i2s_write - i2s_read;                                                                    // Data to be read but not wrapped around the end of the ring buffer
        }
        else
        {
          count = RINGBUFFER_SIZE - i2s_read;                                                              // Data to be read but and wrapped around the end of the ring buffer
        }
        if((i2s_read + count) >= RINGBUFFER_SIZE)
        {
          count = RINGBUFFER_SIZE - i2s_read;                                                              // There is enough data so that emptying it out requires wrapping around the end of the ring buffer
        }
        if(count > 128) count = 128;                                                                       // The "Chunk" size should be <= 128bytes to not overwhelm the UART Tx buffer
        for(uint32_t i=0; i<count; i++)
        {
          while(app_uart_put(ringbuffer[i2s_read + i]) != NRF_SUCCESS);                                    // Load byte-by-byte into UART Tx buffer
          size = i+1;
        }
        i2s_read += size;
        if(i2s_read == RINGBUFFER_SIZE)
        {
          i2s_read = 0;
        }
        if (size != count)
        {
          break;
        }
      }
   }
}

/** @} */
