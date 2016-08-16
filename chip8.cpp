#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <random>
#include "time.h"
#include "chip8.h"

unsigned char font[80] =
{
    0xF0, 0x90, 0x90, 0x90, 0xF0, //0
    0x20, 0x60, 0x20, 0x20, 0x70, //1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, //2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, //3
    0x90, 0x90, 0xF0, 0x10, 0x10, //4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, //5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, //6
    0xF0, 0x10, 0x20, 0x40, 0x40, //7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, //8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, //9
    0xF0, 0x90, 0xF0, 0x90, 0x90, //A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, //B
    0xF0, 0x80, 0x80, 0x80, 0xF0, //C
    0xE0, 0x90, 0x90, 0x90, 0xE0, //D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, //E
    0xF0, 0x80, 0xF0, 0x80, 0x80  //F
};

// Initialize: Set program counter to 0x200, reset opcode, indexRegister, stack pointer, keypad, V
// registers, and clear stack and memory.
void chip8::init() {
    programCounter = 0x200;
    opcode = 0;
    indexRegister = 0;
    stackPointer = 0;

    delayTimer = 0;
    soundTimer = 0;

    // Clear the display
    for (int i = 0; i < 2048; ++i) {
        graphics[i] = 0;
    }

    // Clear stack, keypad, and V registers
    for (int i = 0; i < 16; ++i) {
        stack[i] = 0;
        key[i] = 0;
        V[i] = 0;
    }

    // Clear memory: 4k of memory
    for (int i = 0; i < 4096; ++i) {
        memory[i] = 0;
    }

    // Load font
    for (int i = 0; i < 80; ++i) {
        memory[i] = font[i];
    }

    srand (time(NULL));
}


// Load ROM into memory
bool chip8::load(const char *filePath) {
    init();

    printf("Loading ROM: %s\n", filePath);

    FindexRegisterLE* rom = fopen(filePath, "rb");
    if (rom == NULL) {
        printf("Could not open ROM.");
        return false;
    }

    // Get size of ROM
    fseek(rom, 0, SEEK_END);
    long romSize = ftell(rom);
    rewind(rom);

    // Allocate memory to store ROM
    char* romBuffer = (char*) malloc(sizeof(char) * romSize);
    if (romBuffer == NULL) {
        printf("Could not allocate memory for ROM.");
        return false;
    }

    // Copy ROM into buffer
    size_t result = fread(romBuffer, sizeof(char), (size_t)romSize, rom);
    if (result != romSize) {
        printf("Could not read ROM")
        return false;
    }

    // Copy buffer to memory: load into memory starting at 0x200 (512)
    if ((4096-512) > romSize){
        for (int i = 0; i < romSize; ++i) {
            memory[i + 512] = (uint8_t)romBuffer[i];
        }
    }
    else {
        printf("ROM is too larger")
        return false;
    }

    fclose(rom);
    free(romBuffer);
    return true;
}

// Emulate one cycle
void chip8::cycle() {

    // Fetch op code: 2 bytes
    opcode = memory[programCounter] << 8 | memory[programCounter + 1];

    switch(opcode & 0xF000){

        case 0x0000:

            switch (opcode & 0x000F) {
                // 00E0: Clear screen
                case 0x0000:
                    for (int i = 0; i < 2048; ++i) {
                        graphics[i] = 0;
                    }
                    drawFlag = true;
                    programCounter+=2;
                    break;

                // 00EE: Return from subroutine
                case 0x000E:
                    --stackPointer;
                    programCounter = stack[stackPointer];
                    programCounter += 2;
                    break;

                default:
                    printf("\nUnknown op code: %.4X\n", opcode);
                    exit(3);
            }
            break;

        // 1NNN: Jumps to location NNN
        case 0x1000:
            programCounter = opcode & 0x0FFF;
            break;

        // 2NNN: Calls subroutine at NNN
        case 0x2000:
            stack[stackPointer] = programCounter;
            ++stackPointer;
            programCounter = opcode & 0x0FFF;
            break;

        // 3XNN: Skips the next instruction if VX equals NN.
        case 0x3000:
            if (V[(opcode & 0x0F00) >> 8] == (opcode & 0x00FF))
                programCounter += 4;
            else
                programCounter += 2;
            break;

        // 4XNN: Skips the next instruction if VX does not equal NN.
        case 0x4000:
            if (V[(opcode & 0x0F00) >> 8] != (opcode & 0x00FF))
                programCounter += 4;
            else
                programCounter += 2;
            break;

        // 5XY0: Skips the next instruction if VX equals VY.
        case 0x5000:
            if (V[(opcode & 0x0F00) >> 8] == V[(opcode & 0x00F0) >> 4])
                programCounter += 4;
            else
                programCounter += 2;
            break;

        // 6XNN: Sets VX to NN.
        case 0x6000:
            V[(opcode & 0x0F00) >> 8] = opcode & 0x00FF;
            programCounter += 2;
            break;

        // 7XNN: Adds NN to VX.
        case 0x7000:
            V[(opcode & 0x0F00) >> 8] += opcode & 0x00FF;
            programCounter += 2;
            break;

        // 8XY_
        case 0x8000:
            switch (opcode & 0x000F) {

                // 8XY0: Sets VX to the value of VY.
                case 0x0000:
                    V[(opcode & 0x0F00) >> 8] = V[(opcode & 0x00F0) >> 4];
                    programCounter += 2;
                    break;

                // 8XY1: Sets VX to (VX OR VY).
                case 0x0001:
                    V[(opcode & 0x0F00) >> 8] |= V[(opcode & 0x00F0) >> 4];
                    programCounter += 2;
                    break;

                // 8XY2: Sets VX to (VX AND VY).
                case 0x0002:
                    V[(opcode & 0x0F00) >> 8] &= V[(opcode & 0x00F0) >> 4];
                    programCounter += 2;
                    break;

                // 8XY3: Sets VX to (VX XOR VY).
                case 0x0003:
                    V[(opcode & 0x0F00) >> 8] ^= V[(opcode & 0x00F0) >> 4];
                    programCounter += 2;
                    break;

                // 8XY4: Adds VY to VX. VF is set to 1 when there's a carry,
                // and to 0 when there isn't.
                case 0x0004:
                    V[(opcode & 0x0F00) >> 8] += V[(opcode & 0x00F0) >> 4];
                    if(V[(opcode & 0x00F0) >> 4] > (0xFF - V[(opcode & 0x0F00) >> 8]))
                        V[0xF] = 1; //carry
                    else
                        V[0xF] = 0;
                    programCounter += 2;
                    break;

                // 8XY5: VY is subtracted from VX. VF is set to 0 when
                // there's a borrow, and 1 when there isn't.
                case 0x0005:
                    if(V[(opcode & 0x00F0) >> 4] > V[(opcode & 0x0F00) >> 8])
                        V[0xF] = 0; // borrow
                    else
                        V[0xF] = 1;
                    V[(opcode & 0x0F00) >> 8] -= V[(opcode & 0x00F0) >> 4];
                    programCounter += 2;
                    break;

                // 0x8XY6: Shifts VX right by one. VF is set to the value of
                // the least significant bit of VX before the shift.
                case 0x0006:
                    V[0xF] = V[(opcode & 0x0F00) >> 8] & 0x1;
                    V[(opcode & 0x0F00) >> 8] >>= 1;
                    programCounter += 2;
                    break;

                // 0x8XY7: Sets VX to VY minus VX. VF is set to 0 when there's
                // a borrow, and 1 when there isn't.
                case 0x0007:
                    if(V[(opcode & 0x0F00) >> 8] > V[(opcode & 0x00F0) >> 4])	// VY-VX
                        V[0xF] = 0; // borrow
                    else
                        V[0xF] = 1;
                    V[(opcode & 0x0F00) >> 8] = V[(opcode & 0x00F0) >> 4] - V[(opcode & 0x0F00) >> 8];
                    programCounter += 2;
                    break;

                // 0x8XYE: Shifts VX left by one. VF is set to the value of
                // the most significant bit of VX before the shift.
                case 0x000E:
                    V[0xF] = V[(opcode & 0x0F00) >> 8] >> 7;
                    V[(opcode & 0x0F00) >> 8] <<= 1;
                    programCounter += 2;
                    break;

                default:
                    printf("\nUnknown op code: %.4X\n", opcode);
                    exit(3);
            }
            break;

        // 9XY0: Skips the next instruction if VX doesn't equal VY.
        case 0x9000:
            if (V[(opcode & 0x0F00) >> 8] != V[(opcode & 0x00F0) >> 4])
                programCounter += 4;
            else
                programCounter += 2;
            break;

        // ANNN:  Sets indexRegister to the address NNN.
        case 0xA000:
            indexRegister = opcode & 0x0FFF;
            programCounter += 2;
            break;

        // BNNN : Jumps to the address NNN plus V0.
        case 0xB000:
            programCounter = (opcode & 0x0FFF) + V[0];
            break;

        // CXNN : Sets VX to a random number, masked by NN.
        case 0xC000:
            V[(opcode & 0x0F00) >> 8] = (rand() % 0xFF) & (opcode & 0x00FF);
            programCounter += 2;
            break;

        /* DXYN: Draws an 8 by N sprite at (VX, VY). The row of 8 pixels is read
        from indexRegister. VF is set to 1 if any pixels are flipped from set to unset
        when the sprite is drawn, and to 0 if not.
         */
        case 0xD000:
        {
            unsigned short x = V[(opcode & 0x0F00) >> 8];
            unsigned short y = V[(opcode & 0x00F0) >> 4];
            unsigned short height = opcode & 0x000F;
            unsigned short pixel;

            V[0xF] = 0;
            for (int yline = 0; yline < height; yline++)
            {
                pixel = memory[indexRegister + yline];
                for(int xline = 0; xline < 8; xline++)
                {
                    if((pixel & (0x80 >> xline)) != 0)
                    {
                        if(graphics[(x + xline + ((y + yline) * 64))] == 1)
                        {
                            V[0xF] = 1;
                        }
                        graphics[x + xline + ((y + yline) * 64)] ^= 1;
                    }
                }
            }

            drawFlag = true;
            programCounter += 2;
        }
            break;

        // EX__
        case 0xE000:

            switch (opcode & 0x00FF) {
                // EX9E: Skips the next instruction if the key stored
                // in VX is pressed.
                case 0x009E:
                    if (key[V[(opcode & 0x0F00) >> 8]] != 0)
                        programCounter +=  4;
                    else
                        programCounter += 2;
                    break;

                // EXA1: Skips the next instruction if the key stored
                // in VX isn't pressed.
                case 0x00A1:
                    if (key[V[(opcode & 0x0F00) >> 8]] == 0)
                        programCounter +=  4;
                    else
                        programCounter += 2;
                    break;

                default:
                    printf("\nUnknown op code: %.4X\n", opcode);
                    exit(3);
            }
            break;

        // FX__
        case 0xF000:
            switch(opcode & 0x00FF)
            {
                // FX07: Sets VX to the value of the delay timer
                case 0x0007:
                    V[(opcode & 0x0F00) >> 8] = delayTimer;
                    programCounter += 2;
                    break;

                // FX0A: Wait for keypress, then store in VX
                case 0x000A:
                {
                    bool keyPressed = false;

                    for(int i = 0; i < 16; ++i)
                    {
                        if(key[i] != 0)
                        {
                            V[(opcode & 0x0F00) >> 8] = i;
                            keyPressed = true;
                        }
                    }

                    if(!keyPressed)
                        return;

                    programCounter += 2;
                }
                    break;

                // FX15: Sets the delay timer to VX
                case 0x0015:
                    delayTimer = V[(opcode & 0x0F00) >> 8];
                    programCounter += 2;
                    break;

                // FX18: Sets the sound timer to VX
                case 0x0018:
                    soundTimer = V[(opcode & 0x0F00) >> 8];
                    programCounter += 2;
                    break;

                // FX1E: Adds VX to indexRegister
                case 0x001E:
                    // VF is set to 1 when range overflow (indexRegister+VX>0xFFF), otherwise set to 0
                    if(indexRegister + V[(opcode & 0x0F00) >> 8] > 0xFFF)
                        V[0xF] = 1;
                    else
                        V[0xF] = 0;
                    indexRegister += V[(opcode & 0x0F00) >> 8];
                    programCounter += 2;
                    break;

                // FX29: Sets indexRegister to the location of the sprite for the
                // character in VX. Characters 0-F (hexadecimal) are
                // represented by a 4x5 font
                case 0x0029:
                    indexRegister = V[(opcode & 0x0F00) >> 8] * 0x5;
                    programCounter += 2;
                    break;

                // FX33: Stores the binary representation of VX
                // at the addresses indexRegister, indexRegister plus 1, and indexRegister
                // plus 2
                case 0x0033:
                    memory[indexRegister]     = V[(opcode & 0x0F00) >> 8] / 100;
                    memory[indexRegister + 1] = (V[(opcode & 0x0F00) >> 8] / 10) % 10;
                    memory[indexRegister + 2] = (V[(opcode & 0x0F00) >> 8] % 100) % 10;
                    programCounter += 2;
                    break;

                // FX55: Stores V0 to VX in memory starting at address indexRegister
                case 0x0055:
                    for (int i = 0; i <= ((opcode & 0x0F00) >> 8); ++i)
                        memory[indexRegister + i] = V[i];

                    //  indexRegister = indexRegister + X + 1.
                    indexRegister += ((opcode & 0x0F00) >> 8) + 1;
                    programCounter += 2;
                    break;

                case 0x0065:
                    for (int i = 0; i <= ((opcode & 0x0F00) >> 8); ++i)
                        V[i] = memory[indexRegister + i];

                    // indexRegister = indexRegister + X + 1.
                    indexRegister += ((opcode & 0x0F00) >> 8) + 1;
                    programCounter += 2;
                    break;

            }
            break;

        default:
            printf("\nUnknown op code: %.4X\n", opcode);
            exit(3);
    }


    // Update timers
    if (delayTimer > 0)
        --delayTimer;

    if (soundTimer > 0)
        if(soundTimer == 1);
        --soundTimer;

}

void chip8::render()
{

	for(int y = 0; y < 32; ++y) {
		for(int x = 0; x < 64; ++x) {
			if(graphics[(y*64) + x] == 0)
				printf("O");
			else
				printf(" ");
		}
		printf("\n");
	}
	printf("\n");
}
