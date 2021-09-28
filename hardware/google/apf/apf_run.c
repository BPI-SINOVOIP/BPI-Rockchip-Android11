/*
 * Copyright 2016, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Simple program to try running an APF program against a packet.

#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <libgen.h>
#include <limits.h>
#include <pcap.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "disassembler.h"
#include "apf_interpreter.h"

#define __unused __attribute__((unused))

enum {
    OPT_PROGRAM,
    OPT_PACKET,
    OPT_PCAP,
    OPT_DATA,
    OPT_AGE,
    OPT_TRACE,
};

const struct option long_options[] = {{"program", 1, NULL, OPT_PROGRAM},
                                      {"packet", 1, NULL, OPT_PACKET},
                                      {"pcap", 1, NULL, OPT_PCAP},
                                      {"data", 1, NULL, OPT_DATA},
                                      {"age", 1, NULL, OPT_AGE},
                                      {"trace", 0, NULL, OPT_TRACE},
                                      {"help", 0, NULL, 'h'},
                                      {NULL, 0, NULL, 0}};

// Parses hex in "input". Allocates and fills "*output" with parsed bytes.
// Returns length in bytes of "*output".
size_t parse_hex(const char* input, uint8_t** output) {
    int length = strlen(input);
    if (length & 1) {
        fprintf(stderr, "Argument not even number of characters: %s\n", input);
        exit(1);
    }
    length >>= 1;
    *output = malloc(length);
    if (*output == NULL) {
        fprintf(stderr, "Out of memory, tried to allocate %d\n", length);
        exit(1);
    }
    for (int i = 0; i < length; i++) {
        char byte[3] = { input[i*2], input[i*2+1], 0 };
        char* end_ptr;
        (*output)[i] = strtol(byte, &end_ptr, 16);
        if (end_ptr != byte + 2) {
            fprintf(stderr, "Failed to parse hex %s\n", byte);
            exit(1);
        }
    }
    return length;
}

void print_hex(const uint8_t* input, int len) {
    for (int i = 0; i < len; ++i) {
        printf("%02x", input[i]);
    }
}

int tracing_enabled = 0;

void maybe_print_tracing_header() {
    if (!tracing_enabled) return;

    printf("      R0       R1       PC  Instruction\n");
    printf("-------------------------------------------------\n");

}

// Process packet through APF filter
void packet_handler(uint8_t* program, uint32_t program_len, uint32_t ram_len,
                   const char* pkt, uint32_t filter_age) {
    uint8_t* packet;
    uint32_t packet_len = parse_hex(pkt, &packet);

    maybe_print_tracing_header();

    int ret = accept_packet(program, program_len, ram_len, packet, packet_len,
                            filter_age);
    printf("Packet %sed\n", ret ? "pass" : "dropp");

    free(packet);
}

void apf_trace_hook(uint32_t pc, const uint32_t* regs, const uint8_t* program, uint32_t program_len,
                    const uint8_t* packet __unused, uint32_t packet_len __unused,
                    const uint32_t* memory __unused, uint32_t memory_len __unused) {
    if (!tracing_enabled) return;

    printf("%8" PRIx32 " %8" PRIx32 " ", regs[0], regs[1]);
    apf_disassemble(program, program_len, pc);
}

// Process pcap file through APF filter and generate output files
void file_handler(uint8_t* program, uint32_t program_len, uint32_t ram_len, const char* filename,
                  uint32_t filter_age) {
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t *pcap;
    struct pcap_pkthdr apf_header;
    const uint8_t* apf_packet;
    pcap_dumper_t *passed_dumper, *dropped_dumper;
    const char passed_file[] = "passed.pcap";
    const char dropped_file[] = "dropped.pcap";
    int pass = 0;
    int drop = 0;

    pcap = pcap_open_offline(filename, errbuf);
    if (pcap == NULL) {
        printf("Open pcap file failed.\n");
        exit(1);
    }

    passed_dumper = pcap_dump_open(pcap, passed_file);
    dropped_dumper = pcap_dump_open(pcap, dropped_file);

    if (!passed_dumper || !dropped_dumper) {
        printf("pcap_dump_open(): open output file failed.\n");
        pcap_close(pcap);
        exit(1);
    }

    while ((apf_packet = pcap_next(pcap, &apf_header)) != NULL) {
        maybe_print_tracing_header();

        int result = accept_packet(program, program_len, ram_len, apf_packet,
                                  apf_header.len, filter_age);

        if (!result){
            drop++;
            pcap_dump((u_char*)dropped_dumper, &apf_header, apf_packet);
        } else {
            pass++;
            pcap_dump((u_char*)passed_dumper, &apf_header, apf_packet);
        }
    }

    printf("%d packets dropped\n", drop);
    printf("%d packets passed\n", pass);
    pcap_dump_close(passed_dumper);
    pcap_dump_close(dropped_dumper);
    pcap_close(pcap);
}

void print_usage(char* cmd) {
    fprintf(stderr,
            "Usage: %s --program <program> --pcap <file>|--packet <packet> "
            "[--data <content>] [--age <number>] [--trace]\n"
            "  --program    APF program, in hex.\n"
            "  --pcap       Pcap file to run through program.\n"
            "  --packet     Packet to run through program.\n"
            "  --data       Data memory contents, in hex.\n"
            "  --age        Age of program in seconds (default: 0).\n"
            "  --trace      Enable APF interpreter debug tracing\n"
            "  -h, --help   Show this message.\n",
            basename(cmd));
}

int main(int argc, char* argv[]) {
    if (argc > 9) {
        print_usage(argv[0]);
        exit(1);
    }

    uint8_t* program = NULL;
    uint32_t program_len;
    const char* filename = NULL;
    char* packet = NULL;
    uint8_t* data = NULL;
    uint32_t data_len = 0;
    uint32_t filter_age = 0;

    int opt;
    char *endptr;

    while ((opt = getopt_long_only(argc, argv, "h", long_options, NULL)) != -1) {
        switch (opt) {
            case OPT_PROGRAM:
                program_len = parse_hex(optarg, &program);
                break;
            case OPT_PACKET:
                if (!program) {
                    printf("<packet> requires <program> first\n\'%s -h or --help\' "
                           "for more information\n", basename(argv[0]));
                    exit(1);
                }
                if (filename) {
                    printf("Cannot use <file> with <packet> \n\'%s -h or --help\' "
                           "for more information\n", basename(argv[0]));

                    exit(1);
                }
                packet = optarg;
                break;
            case OPT_PCAP:
                if (!program) {
                    printf("<file> requires <program> first\n\'%s -h or --help\' "
                           "for more information\n", basename(argv[0]));

                    exit(1);
                }
                if (packet) {
                    printf("Cannot use <packet> with <file>\n\'%s -h or --help\' "
                           "for more information\n", basename(argv[0]));

                    exit(1);
                }
                filename = optarg;
                break;
            case OPT_DATA:
                data_len = parse_hex(optarg, &data);
                break;
            case OPT_AGE:
                errno = 0;
                filter_age = strtoul(optarg, &endptr, 10);
                if ((errno == ERANGE && filter_age == UINT32_MAX) ||
                    (errno != 0 && filter_age == 0)) {
                    perror("Error on age option: strtoul");
                    exit(1);
                }
                if (endptr == optarg) {
                    printf("No digit found in age.\n");
                    exit(1);
                }
                break;
            case OPT_TRACE:
                tracing_enabled = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                exit(0);
                break;
            default:
                print_usage(argv[0]);
                exit(1);
                break;
        }
    }

    if (!program) {
        printf("Must have APF program in option.\n");
        exit(1);
    }

    if (!filename && !packet) {
        printf("Missing file or packet after program.\n");
        exit(1);
    }

    // Combine the program and data into the unified APF buffer.
    if (data) {
        program = realloc(program, program_len + data_len);
        memcpy(program + program_len, data, data_len);
        free(data);
    }

    uint32_t ram_len = program_len + data_len;

    if (filename)
        file_handler(program, program_len, ram_len, filename, filter_age);
    else
        packet_handler(program, program_len, ram_len, packet, filter_age);

    if (data_len) {
        printf("Data: ");
        print_hex(program + program_len, data_len);
        printf("\n");
    }

    free(program);
    return 0;
}
