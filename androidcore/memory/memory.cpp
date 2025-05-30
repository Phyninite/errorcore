#include "memory.hpp"

#include <thread>
#include <cstdio>
#include <string>
#include <fstream>
#include <iostream>
#include <dlfcn.h>
#include <cstdint>

static uint64_t base;

auto memory::findLibrary(const char* library) -> std::uint64_t
{
	char filename[0xFF] = { 0 },
		buffer[1024] = { 0 };
	FILE* fp = NULL;
	std::uint64_t address = 0;

	sprintf(filename, "/proc/self/maps");

	fp = fopen(filename, "rt");
	if (fp == NULL) {
		perror("fopen");
		goto done;
	}

	while (fgets(buffer, sizeof(buffer), fp)) {
		if (strstr(buffer, library)) {
			address = static_cast<std::uint64_t>(strtoul(buffer, NULL, 16));
			goto done;
		}
	}

done:

	if (fp) {
		fclose(fp);
	}

	return address;
}

auto memory::rebaseAddress(std::uint64_t relativeAddr) -> std::uint64_t
{
	base = findLibrary(TARGET_LIB);
	if (base == 0)
		return 0;
	return base + relativeAddr;
}
