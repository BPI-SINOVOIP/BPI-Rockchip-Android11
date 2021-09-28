
// This is required on Mac OS X for getting PRI* macros #defined.
#define __STDC_FORMAT_MACROS

#include <assert.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

const size_t mem_size = (1 << 30)/4;
const int toggles = 540000;

const uint64_t pte_pattern = 0xb7d02580000003C5;
char *g_mem;

// Pick a random page in the memory region.
uint8_t* pick_addr(uint8_t* area_base, uint64_t mem_size) {
  size_t offset = (rand() << 12) % mem_size;
  return area_base + offset;
}

class Timer {
  struct timeval start_time_;

 public:
  Timer() {
    // Note that we use gettimeofday() (with microsecond resolution)
    // rather than clock_gettime() (with nanosecond resolution) so
    // that this works on Mac OS X, because OS X doesn't provide
    // clock_gettime() and we don't really need nanosecond resolution.
    int rc = gettimeofday(&start_time_, NULL);
    assert(rc == 0);
  }

  double get_diff() {
    struct timeval end_time;
    int rc = gettimeofday(&end_time, NULL);
    assert(rc == 0);
    return (end_time.tv_sec - start_time_.tv_sec
            + (double) (end_time.tv_usec - start_time_.tv_usec) / 1e6);
  }

  void print_iters(uint64_t iterations) {
    double total_time = get_diff();
    double iter_time = total_time / iterations;
    printf("%.3fns,%g,%" PRIu64,
           iter_time * 1e9, total_time, iterations);
  }
};

uint64_t get_physical_address(uint64_t virtual_address) {
  int fd = open("/proc/self/pagemap", O_RDONLY);
  assert(fd >=0);

  off_t pos = lseek(fd, (virtual_address / 0x1000) * 8, SEEK_SET);
  assert(pos >= 0);
  uint64_t value;
  int got = read(fd, &value, 8);

  close(fd);
  assert(got == 8);
  return ((value & ((1ULL << 54)-1)) * 0x1000) | 
    (virtual_address & 0xFFF);
}

static int sigint = 0;
static void sigint_handler(int signum) {
  sigint = 1;
}
static int sigquit = 0;
static void sigquit_handler(int signum) {
  sigint = 1;
  sigquit = 1;
}

static void toggle(int iterations, int addr_count) {
  Timer t;
  for (int j = 0; j < iterations; j++) {
    volatile uint32_t *addrs[addr_count];
    for (int a = 0; a < addr_count; a++) {
      addrs[a] = (uint32_t *) pick_addr((uint8_t*)g_mem, mem_size);
      //printf(" Hammering virtual address %16lx, physical address %16lx\n",
      //(uint64_t)addrs[a], get_physical_address((uint64_t)addrs[a]));
    }

    // TODO(wad) try the approach from github.com/CMU-SAFARI/rowhammer
    //           as it may be faster.
    uint32_t sum = 0;
    for (int i = 0; i < toggles; i++) {
      for (int a = 0; a < addr_count; a++)
        sum += *addrs[a] + 1;
      for (int a = 0; a < addr_count; a++)
        asm volatile("clflush (%0)" : : "r" (addrs[a]) : "memory");
    }
    if (sigint) {
      sigint = 0;
      break;
    }
  }
  t.print_iters(iterations * addr_count * toggles);
}

void main_prog() {
  g_mem = (char *) mmap(NULL, mem_size, PROT_READ | PROT_WRITE,
                        MAP_ANON | MAP_PRIVATE, -1, 0);
  assert(g_mem != MAP_FAILED);

  // printf("clear\n");

  //memset(g_mem, 0xff, mem_size);

  // Fill memory with pattern that resembles page tables entries.
  // c5 03 00 00 80 25 d0 b7
 for (uint32_t i = 0; i < mem_size; i += 8) {
    uint64_t* ptr = (uint64_t*)(&g_mem[i]);
    *ptr = pte_pattern;
  }

  Timer t;
  int iter = 0;
  for (;;) {
    printf("%d,%.2fs,", iter++, t.get_diff());
    fflush(stdout);
    toggle(3000, 4);

    Timer check_timer;
    // printf("check\n");
    uint64_t *end = (uint64_t *) (g_mem + mem_size);
    uint64_t *ptr;
    int errors = 0;
    for (ptr = (uint64_t *) g_mem; ptr < end; ptr++) {
      uint64_t got = *ptr;
      if (got != pte_pattern) {
        fprintf(stderr, "error at %p (%16lx): got 0x%" PRIx64 "\n", ptr, get_physical_address((uint64_t)ptr), got);
        fprintf(stderr, "after %.2fs\n", t.get_diff());
        errors++;
      }
    }
    printf(",%fs", check_timer.get_diff());
    fflush(stdout);
    if (errors) {
      printf(",%d\n", errors);
      fflush(stdout);
      exit(1);
    }
    printf(",0\n");
    fflush(stdout);
    if (sigquit)
      break;
  }
}


int main(int argc, char **argv) {
  // In case we are running as PID 1, we fork() a subprocess to run
  // the test in.  Otherwise, if process 1 exits or crashes, this will
  // cause a kernel panic (which can cause a reboot or just obscure
  // log output and prevent console scrollback from working).
  // Output should look like:
  // [iteration #],[relative start offset in sec],[itertime in ns],[total time in s],[iteration count],[check time in s],[error count]
  signal(SIGINT, &sigint_handler);
  signal(SIGQUIT, &sigquit_handler);
  int pid = fork();
  if (pid == 0) {
    main_prog();
    _exit(1);
  }

  int status;
  int sec = argc == 2 ? atoi(argv[1]) : 60*60;
  while (sec--) {
    if (sigint) {
      sigint = 0;
      kill(pid, SIGINT);
      if (sigquit)
        break;
    }
    if (waitpid(pid, &status, WNOHANG) == pid) {
      printf("** exited with status %i (0x%x)\n", status, status);
      exit(status);
    }
    sleep(1);
  }
  kill(pid, SIGQUIT);
  sleep(1);
  kill(pid, SIGKILL);
  // Let init reap.
  return 0;
}
