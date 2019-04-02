#include <stdint.h>
union Endian
{
    uint16_t s;
    char c[sizeof(uint16_t)];
};
static Endian un = { 0x0102 };
static bool HostIsBigEdian() {
    if (un.c[0] == 1)
        return true;
    else
        return false;
}

int main()
{
	return HostIsBigEdian();
}