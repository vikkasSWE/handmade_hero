#include <windows.h>

void foo(void);

int CALLBACK WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdShow,
    int nCmdShow)
{

    foo();
}

void foo(void)
{
    char test_1 = 0xF;
    test_1 += 1;

    short test_2 = 0xF;
    test_2 += 1;

    long test_3 = 0xF;
    test_3 += 1;

    long long test_4 = 0xF;
    test_4 += 1;
}