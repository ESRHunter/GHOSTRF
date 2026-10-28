#pragma once
// Lightweight SPI compatibility shim for host compilation
class SPIClass {};
#ifndef SPI_MOCK_INSTANCE_DEFINED
#define SPI_MOCK_INSTANCE_DEFINED
inline SPIClass SPI;
#endif
