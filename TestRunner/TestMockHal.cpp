#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "mock_hal.h"
#include <string.h>

TEST_CASE("HAL_CAN_AddTxMessage Standard ID") {
    CAN_TxHeaderTypeDef header;
    uint8_t data[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    uint32_t mailbox;

    header.StdId = 0x123;
    header.IDE = 0; // Standard ID
    header.DLC = 8;

    CHECK(HAL_CAN_AddTxMessage(NULL, &header, data, &mailbox) == 0);
    CHECK(get_can_tx_buffer_count() == 1);
    CAN_TxMessage_t msg = get_can_tx_message(0);
    CHECK(msg.TxHeader.StdId == 0x123);
    CHECK(msg.TxHeader.IDE == 0);
    CHECK(msg.TxHeader.DLC == 8);
    CHECK(memcmp(msg.pData, data, 8) == 0);
    clear_can_tx_buffer();
    CHECK(get_can_tx_buffer_count() == 0);
}

TEST_CASE("HAL_CAN_AddTxMessage Extended ID") {
    CAN_TxHeaderTypeDef header;
    uint8_t data[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    uint32_t mailbox;

    header.ExtId = 0x1234567;
    header.IDE = 1; // Extended ID
    header.DLC = 8;

    CHECK(HAL_CAN_AddTxMessage(NULL, &header, data, &mailbox) == 0);
    CHECK(get_can_tx_buffer_count() == 1);
    CAN_TxMessage_t msg = get_can_tx_message(0);
    CHECK(msg.TxHeader.ExtId == 0x1234567);
    CHECK(msg.TxHeader.IDE == 1);
    CHECK(msg.TxHeader.DLC == 8);
    CHECK(memcmp(msg.pData, data, 8) == 0);
    clear_can_tx_buffer();
    CHECK(get_can_tx_buffer_count() == 0);
}

TEST_CASE("HAL_CAN_GetRxMessage Standard ID") {
     CAN_RxHeaderTypeDef header;
    uint8_t data[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    uint8_t rx_data[8];

    header.StdId = 0x123;
    header.IDE = 0;
    header.DLC = 8;

    inject_can_rx_message(header, data);

    CHECK(HAL_CAN_GetRxMessage(NULL, 0, &header, rx_data) == 0);
    CHECK(header.StdId == 0x123);
    CHECK(header.IDE == 0);
    CHECK(header.DLC == 8);
    CHECK(memcmp(rx_data, data, 8) == 0);
}


TEST_CASE("HAL_CAN_GetRxMessage Extended ID") {
     CAN_RxHeaderTypeDef header;
    uint8_t data[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    uint8_t rx_data[8];

    header.ExtId = 0x1234567;
    header.IDE = 1;
    header.DLC = 8;

    inject_can_rx_message(header, data);

    CHECK(HAL_CAN_GetRxMessage(NULL, 0, &header, rx_data) == 0);
    CHECK(header.ExtId == 0x1234567);
    CHECK(header.IDE == 1);
    CHECK(header.DLC == 8);
    CHECK(memcmp(rx_data, data, 8) == 0);
}

TEST_CASE("HAL_CAN_GetTxMailboxesFreeLevel") {
    set_current_free_mailboxes(1);
    CHECK(HAL_CAN_GetTxMailboxesFreeLevel(NULL) == 1);
    set_current_free_mailboxes(3);
    CHECK(HAL_CAN_GetTxMailboxesFreeLevel(NULL) == 3);
}


TEST_CASE("HAL_CAN_ConfigFilter"){
    CAN_FilterTypeDef filter;
    CHECK(HAL_CAN_ConfigFilter(NULL, &filter) == 0);
}

TEST_CASE("HAL_CAN_GetRxFifoFillLevel"){
    set_current_rx_fifo_fill_level(1);
    CHECK(HAL_CAN_GetRxFifoFillLevel(NULL,0) == 1);
    set_current_rx_fifo_fill_level(0);
    CHECK(HAL_CAN_GetRxFifoFillLevel(NULL,0) == 0);
}


TEST_CASE("HAL_UART_Transmit") {
    UART_HandleTypeDef huart;
    uint8_t data[] = "test data";
    CHECK(HAL_UART_Transmit(&huart, data, sizeof(data)-1, 1000) == 0);
    CHECK(get_uart_tx_buffer_count() == 9);
    CHECK(memcmp(get_uart_tx_buffer(), data, sizeof(data)-1) == 0);
    clear_uart_tx_buffer();
    CHECK(get_uart_tx_buffer_count() == 0);
}

TEST_CASE("HAL_UART_Transmit_DMA") {
    UART_HandleTypeDef huart;
    uint8_t data[] = "test data";
    CHECK(HAL_UART_Transmit_DMA(&huart, data, sizeof(data)-1) == 0);
    CHECK(get_uart_tx_buffer_count() == 9);
    CHECK(memcmp(get_uart_tx_buffer(), data, sizeof(data)-1) == 0);
    clear_uart_tx_buffer();
    CHECK(get_uart_tx_buffer_count() == 0);
}


TEST_CASE("HAL_UART_Receive timeout") {
    UART_HandleTypeDef huart;
    uint8_t expected_data[] = "hello";
    uint8_t recv_buffer[10];

    // Simulate a situation where only some bytes are received before timeout
    inject_uart_rx_data(expected_data, 2); // Only Injecting 2 bytes
    set_current_tick(0); // set mock time to 0

    CHECK(HAL_UART_Receive(&huart, recv_buffer, 5, 100) == 1); // we should get timeout
    CHECK(memcmp(recv_buffer, expected_data, 2) == 0); //Check the 2 bytes we received
    clear_uart_rx_buffer();
}

TEST_CASE("HAL_UART_Receive no timeout") {
     UART_HandleTypeDef huart;
    uint8_t expected_data[] = "hello";
    uint8_t recv_buffer[10];

    // Simulate a situation where all bytes are received
    inject_uart_rx_data(expected_data, 5); // Inject all bytes
    set_current_tick(0); // set mock time to 0

    CHECK(HAL_UART_Receive(&huart, recv_buffer, 5, 100) == 0); // we should not get timeout
    CHECK(memcmp(recv_buffer, expected_data, 5) == 0); //Check the 5 bytes we received
    clear_uart_rx_buffer();
}


TEST_CASE("HAL_UART_Receive_DMA no timeout") {
    UART_HandleTypeDef huart;
    uint8_t expected_data[] = "hello";
    uint8_t recv_buffer[10];

    // Simulate a situation where all bytes are received
    inject_uart_rx_data(expected_data, 5); // Inject all bytes
    set_current_tick(0);
    CHECK(HAL_UART_Receive_DMA(&huart, recv_buffer, 5) == 0); // we should not get timeout
    CHECK(memcmp(recv_buffer, expected_data, 5) == 0); //Check the 5 bytes we received
    clear_uart_rx_buffer();
}


TEST_CASE("HAL_UART_Receive_DMA partial") {
    UART_HandleTypeDef huart;
    uint8_t expected_data[] = "hello";
    uint8_t recv_buffer[10];

    // Simulate a situation where all bytes are received
    inject_uart_rx_data(expected_data, 2); // Inject 2 bytes
    set_current_tick(0);
    CHECK(HAL_UART_Receive_DMA(&huart, recv_buffer, 5) == 1); // we should get an error because not all bytes are available
    CHECK(memcmp(recv_buffer, expected_data, 2) == 0); //Check the 2 bytes we received
    clear_uart_rx_buffer();
}

TEST_CASE("HAL_Delay"){
    set_current_tick(0);
    HAL_Delay(100);
    CHECK(HAL_GetTick() == 100);
    HAL_Delay(200);
    CHECK(HAL_GetTick() == 300);
}


TEST_CASE("HAL_GetTick"){
    set_current_tick(10);
    CHECK(HAL_GetTick() == 10);
    set_current_tick(20);
    CHECK(HAL_GetTick() == 20);
}

TEST_CASE("HAL_I2C_Master_Transmit") {
    I2C_HandleTypeDef hi2c;
    uint8_t data[] = {0x12, 0x34, 0x56};
    CHECK(HAL_I2C_Master_Transmit(&hi2c, 0x50, data, 3, 100) == 0);
}

TEST_CASE("HAL_I2C_Mem_Read success") {
    I2C_HandleTypeDef hi2c;
    uint8_t expected_data[] = {0xAA, 0xBB, 0xCC};
    uint8_t read_data[3];
    inject_i2c_mem_data(0x50, 0x10, expected_data, 3);

    CHECK(HAL_I2C_Mem_Read(&hi2c, 0x50, 0x10, 1, read_data, 3, 100) == 0);
    CHECK(memcmp(read_data, expected_data, 3) == 0);
    clear_i2c_mem_data();
}


TEST_CASE("HAL_I2C_Mem_Read Fail invalid address") {
    I2C_HandleTypeDef hi2c;
    uint8_t expected_data[] = {0xAA, 0xBB, 0xCC};
    uint8_t read_data[3];
    inject_i2c_mem_data(0x50, 0x10, expected_data, 3);
    
    CHECK(HAL_I2C_Mem_Read(&hi2c, 0x51, 0x10, 1, read_data, 3, 100) == 1);
    clear_i2c_mem_data();
}

TEST_CASE("HAL_I2C_Mem_Read Fail invalid size") {
    I2C_HandleTypeDef hi2c;
     uint8_t expected_data[] = {0xAA, 0xBB, 0xCC};
    uint8_t read_data[4];
    inject_i2c_mem_data(0x50, 0x10, expected_data, 3);
    
    CHECK(HAL_I2C_Mem_Read(&hi2c, 0x50, 0x10, 1, read_data, 4, 100) == 1);
     clear_i2c_mem_data();
}



TEST_CASE("HAL_I2C_Mem_Write") {
    I2C_HandleTypeDef hi2c;
    uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    CHECK(HAL_I2C_Mem_Write(&hi2c, 0x50, 0x20, 1, data, 4, 100) == 0);
}