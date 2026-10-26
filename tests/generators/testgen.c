#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLOCK_SIZE 65536
#define BLOCK_COUNT 32

int main(void)
{
    const char *filename = "tests/reference/reference_big.bin";

    FILE *f = fopen(filename, "wb");

    if (!f) {
        perror("Cannot create test file");
        return 1;
    }

    unsigned char *block = malloc(BLOCK_SIZE);

    if (!block) {
        perror("Cannot allocate memory");
        fclose(f);
        return 1;
    }

    /*
     * Создаём один большой уникальный блок.
     * Затем записываем его много раз.
     *
     * Это специально сделано для проверки
     * механизма REFERENCE.
     */

    for (size_t i = 0; i < BLOCK_SIZE; i++) {
        block[i] = (unsigned char)(
            (i * 37 + 17) ^
            ((i >> 8) * 13)
        );
    }

    for (int n = 0; n < BLOCK_COUNT; n++) {

        /*
         * Каждый второй блок слегка отличается.
         * Это позволит проверить, умеет ли ANLC
         * находить не только полные повторы.
         */

        if (n % 8 == 0) {
            block[100] ^= (unsigned char)n;
            block[1000] ^= (unsigned char)(n * 3);
        }

        fwrite(block, 1, BLOCK_SIZE, f);

        if (n % 8 == 0) {
            block[100] ^= (unsigned char)n;
            block[1000] ^= (unsigned char)(n * 3);
        }
    }

    free(block);
    fclose(f);

    printf("ANL TEST FILE GENERATOR\n");
    printf("========================\n");
    printf("Output: %s\n", filename);
    printf("Block size: %d bytes\n", BLOCK_SIZE);
    printf("Blocks: %d\n", BLOCK_COUNT);
    printf("Total size: %d bytes\n",
           BLOCK_SIZE * BLOCK_COUNT);

    printf("\nTest file created successfully.\n");

    return 0;
}