/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "LibBpfLoader"

#include <errno.h>
#include <linux/bpf.h>
#include <linux/elf.h>
#include <log/log.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>

#include "../progs/include/bpf_map_def.h"
#include "LoaderUtils.h"
#include "bpf/BpfUtils.h"
#include "include/libbpf_android.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include <android-base/properties.h>
#include <android-base/strings.h>
#include <android-base/unique_fd.h>

#define BPF_FS_PATH "/sys/fs/bpf/"

// Size of the BPF log buffer for verifier logging
#define BPF_LOAD_LOG_SZ 0x1ffff

using android::base::StartsWith;
using android::base::unique_fd;
using std::ifstream;
using std::ios;
using std::optional;
using std::string;
using std::vector;

namespace android {
namespace bpf {

typedef struct {
    const char* name;
    enum bpf_prog_type type;
} sectionType;

/*
 * Map section name prefixes to program types, the section name will be:
 * SEC(<prefix>/<name-of-program>)
 * For example:
 * SEC("tracepoint/sched_switch_func") where sched_switch_funcs
 * is the name of the program, and tracepoint is the type.
 */
sectionType sectionNameTypes[] = {
    {"kprobe", BPF_PROG_TYPE_KPROBE},
    {"tracepoint", BPF_PROG_TYPE_TRACEPOINT},
    {"skfilter", BPF_PROG_TYPE_SOCKET_FILTER},
    {"cgroupskb", BPF_PROG_TYPE_CGROUP_SKB},
    {"schedcls", BPF_PROG_TYPE_SCHED_CLS},
    {"cgroupsock", BPF_PROG_TYPE_CGROUP_SOCK},

    /* End of table */
    {"END", BPF_PROG_TYPE_UNSPEC},
};

typedef struct {
    enum bpf_prog_type type;
    string name;
    vector<char> data;
    vector<char> rel_data;
    optional<struct bpf_prog_def> prog_def;

    unique_fd prog_fd; /* fd after loading */
} codeSection;

static int readElfHeader(ifstream& elfFile, Elf64_Ehdr* eh) {
    elfFile.seekg(0);
    if (elfFile.fail()) return -1;

    if (!elfFile.read((char*)eh, sizeof(*eh))) return -1;

    return 0;
}

/* Reads all section header tables into an Shdr array */
static int readSectionHeadersAll(ifstream& elfFile, vector<Elf64_Shdr>& shTable) {
    Elf64_Ehdr eh;
    int ret = 0;

    ret = readElfHeader(elfFile, &eh);
    if (ret) return ret;

    elfFile.seekg(eh.e_shoff);
    if (elfFile.fail()) return -1;

    /* Read shdr table entries */
    shTable.resize(eh.e_shnum);

    if (!elfFile.read((char*)shTable.data(), (eh.e_shnum * eh.e_shentsize))) return -ENOMEM;

    return 0;
}

/* Read a section by its index - for ex to get sec hdr strtab blob */
static int readSectionByIdx(ifstream& elfFile, int id, vector<char>& sec) {
    vector<Elf64_Shdr> shTable;
    int entries, ret = 0;

    ret = readSectionHeadersAll(elfFile, shTable);
    if (ret) return ret;
    entries = shTable.size();

    elfFile.seekg(shTable[id].sh_offset);
    if (elfFile.fail()) return -1;

    sec.resize(shTable[id].sh_size);
    if (!elfFile.read(sec.data(), shTable[id].sh_size)) return -1;

    return 0;
}

/* Read whole section header string table */
static int readSectionHeaderStrtab(ifstream& elfFile, vector<char>& strtab) {
    Elf64_Ehdr eh;
    int ret = 0;

    ret = readElfHeader(elfFile, &eh);
    if (ret) return ret;

    ret = readSectionByIdx(elfFile, eh.e_shstrndx, strtab);
    if (ret) return ret;

    return 0;
}

/* Get name from offset in strtab */
static int getSymName(ifstream& elfFile, int nameOff, string& name) {
    int ret;
    vector<char> secStrTab;

    ret = readSectionHeaderStrtab(elfFile, secStrTab);
    if (ret) return ret;

    if (nameOff >= (int)secStrTab.size()) return -1;

    name = string((char*)secStrTab.data() + nameOff);
    return 0;
}

/* Reads a full section by name - example to get the GPL license */
static int readSectionByName(const char* name, ifstream& elfFile, vector<char>& data) {
    vector<char> secStrTab;
    vector<Elf64_Shdr> shTable;
    int ret;

    ret = readSectionHeadersAll(elfFile, shTable);
    if (ret) return ret;

    ret = readSectionHeaderStrtab(elfFile, secStrTab);
    if (ret) return ret;

    for (int i = 0; i < (int)shTable.size(); i++) {
        char* secname = secStrTab.data() + shTable[i].sh_name;
        if (!secname) continue;

        if (!strcmp(secname, name)) {
            vector<char> dataTmp;
            dataTmp.resize(shTable[i].sh_size);

            elfFile.seekg(shTable[i].sh_offset);
            if (elfFile.fail()) return -1;

            if (!elfFile.read((char*)dataTmp.data(), shTable[i].sh_size)) return -1;

            data = dataTmp;
            return 0;
        }
    }
    return -2;
}

static int readSectionByType(ifstream& elfFile, int type, vector<char>& data) {
    int ret;
    vector<Elf64_Shdr> shTable;

    ret = readSectionHeadersAll(elfFile, shTable);
    if (ret) return ret;

    for (int i = 0; i < (int)shTable.size(); i++) {
        if ((int)shTable[i].sh_type != type) continue;

        vector<char> dataTmp;
        dataTmp.resize(shTable[i].sh_size);

        elfFile.seekg(shTable[i].sh_offset);
        if (elfFile.fail()) return -1;

        if (!elfFile.read((char*)dataTmp.data(), shTable[i].sh_size)) return -1;

        data = dataTmp;
        return 0;
    }
    return -2;
}

static bool symCompare(Elf64_Sym a, Elf64_Sym b) {
    return (a.st_value < b.st_value);
}

static int readSymTab(ifstream& elfFile, int sort, vector<Elf64_Sym>& data) {
    int ret, numElems;
    Elf64_Sym* buf;
    vector<char> secData;

    ret = readSectionByType(elfFile, SHT_SYMTAB, secData);
    if (ret) return ret;

    buf = (Elf64_Sym*)secData.data();
    numElems = (secData.size() / sizeof(Elf64_Sym));
    data.assign(buf, buf + numElems);

    if (sort) std::sort(data.begin(), data.end(), symCompare);
    return 0;
}

static enum bpf_prog_type getSectionType(string& name) {
    for (int i = 0; sectionNameTypes[i].type != BPF_PROG_TYPE_UNSPEC; i++)
        if (StartsWith(name, sectionNameTypes[i].name)) return sectionNameTypes[i].type;

    return BPF_PROG_TYPE_UNSPEC;
}

/* If ever needed
static string getSectionName(enum bpf_prog_type type)
{
    for (int i = 0; sectionNameTypes[i].type != BPF_PROG_TYPE_UNSPEC; i++)
        if (sectionNameTypes[i].type == type)
            return std::string(sectionNameTypes[i].name);

    return NULL;
}
*/

static bool isRelSection(codeSection& cs, string& name) {
    for (int i = 0; sectionNameTypes[i].type != BPF_PROG_TYPE_UNSPEC; i++) {
        sectionType st = sectionNameTypes[i];

        if (st.type != cs.type) continue;

        if (StartsWith(name, std::string(".rel") + st.name + "/"))
            return true;
        else
            return false;
    }
    return false;
}

static int readProgDefs(ifstream& elfFile, vector<struct bpf_prog_def>& pd) {
    vector<char> pdData;
    int ret = readSectionByName("progs", elfFile, pdData);
    if (ret == -2) return 0;
    if (ret) return ret;

    pd.resize(pdData.size() / sizeof(struct bpf_prog_def));
    memcpy(pd.data(), pdData.data(), pdData.size());
    return 0;
}

static int getSectionSymNames(ifstream& elfFile, const string& sectionName, vector<string>& names) {
    int ret;
    string name;
    vector<Elf64_Sym> symtab;
    vector<Elf64_Shdr> shTable;

    ret = readSymTab(elfFile, 1 /* sort */, symtab);
    if (ret) return ret;

    /* Get index of section */
    ret = readSectionHeadersAll(elfFile, shTable);
    if (ret) return ret;

    int sec_idx = -1;
    for (int i = 0; i < (int)shTable.size(); i++) {
        ret = getSymName(elfFile, shTable[i].sh_name, name);
        if (ret) return ret;

        if (!name.compare(sectionName)) {
            sec_idx = i;
            break;
        }
    }

    /* No section found with matching name*/
    if (sec_idx == -1) {
        ALOGE("No %s section could be found in elf object\n", sectionName.c_str());
        return -1;
    }

    for (int i = 0; i < (int)symtab.size(); i++) {
        if (symtab[i].st_shndx == sec_idx) {
            string s;
            ret = getSymName(elfFile, symtab[i].st_name, s);
            if (ret) return ret;
            names.push_back(s);
        }
    }

    return 0;
}

/* Read a section by its index - for ex to get sec hdr strtab blob */
static int readCodeSections(ifstream& elfFile, vector<codeSection>& cs) {
    vector<Elf64_Shdr> shTable;
    int entries, ret = 0;

    ret = readSectionHeadersAll(elfFile, shTable);
    if (ret) return ret;
    entries = shTable.size();

    vector<struct bpf_prog_def> pd;
    ret = readProgDefs(elfFile, pd);
    if (ret) return ret;
    vector<string> progDefNames;
    ret = getSectionSymNames(elfFile, "progs", progDefNames);
    if (!pd.empty() && ret) return ret;

    for (int i = 0; i < entries; i++) {
        string name;
        codeSection cs_temp;
        cs_temp.type = BPF_PROG_TYPE_UNSPEC;

        ret = getSymName(elfFile, shTable[i].sh_name, name);
        if (ret) return ret;

        enum bpf_prog_type ptype = getSectionType(name);
        if (ptype != BPF_PROG_TYPE_UNSPEC) {
            string oldName = name;
            deslash(name);
            cs_temp.type = ptype;
            cs_temp.name = name;

            ret = readSectionByIdx(elfFile, i, cs_temp.data);
            if (ret) return ret;
            ALOGD("Loaded code section %d (%s)\n", i, name.c_str());

            vector<string> csSymNames;
            ret = getSectionSymNames(elfFile, oldName, csSymNames);
            if (ret || !csSymNames.size()) return ret;
            for (size_t i = 0; i < progDefNames.size(); ++i) {
                if (!progDefNames[i].compare(csSymNames[0] + "_def")) {
                    cs_temp.prog_def = pd[i];
                    break;
                }
            }
        }

        /* Check for rel section */
        if (cs_temp.data.size() > 0 && i < entries) {
            ret = getSymName(elfFile, shTable[i + 1].sh_name, name);
            if (ret) return ret;

            if (isRelSection(cs_temp, name)) {
                ret = readSectionByIdx(elfFile, i + 1, cs_temp.rel_data);
                if (ret) return ret;
                ALOGD("Loaded relo section %d (%s)\n", i, name.c_str());
            }
        }

        if (cs_temp.data.size() > 0) {
            cs.push_back(std::move(cs_temp));
            ALOGD("Adding section %d to cs list\n", i);
        }
    }
    return 0;
}

static int getSymNameByIdx(ifstream& elfFile, int index, string& name) {
    vector<Elf64_Sym> symtab;
    int ret = 0;

    ret = readSymTab(elfFile, 0 /* !sort */, symtab);
    if (ret) return ret;

    if (index >= (int)symtab.size()) return -1;

    return getSymName(elfFile, symtab[index].st_name, name);
}

static int createMaps(const char* elfPath, ifstream& elfFile, vector<unique_fd>& mapFds) {
    int ret;
    vector<char> mdData;
    vector<struct bpf_map_def> md;
    vector<string> mapNames;
    string fname = pathToFilename(string(elfPath), true);

    ret = readSectionByName("maps", elfFile, mdData);
    if (ret == -2) return 0;  // no maps to read
    if (ret) return ret;
    md.resize(mdData.size() / sizeof(struct bpf_map_def));
    memcpy(md.data(), mdData.data(), mdData.size());

    ret = getSectionSymNames(elfFile, "maps", mapNames);
    if (ret) return ret;

    for (int i = 0; i < (int)mapNames.size(); i++) {
        unique_fd fd;
        // Format of pin location is /sys/fs/bpf/map_<filename>_<mapname>
        string mapPinLoc;
        bool reuse = false;

        mapPinLoc = string(BPF_FS_PATH) + "map_" + fname + "_" + string(mapNames[i]);
        if (access(mapPinLoc.c_str(), F_OK) == 0) {
            fd.reset(bpf_obj_get(mapPinLoc.c_str()));
            ALOGD("bpf_create_map reusing map %s, ret: %d\n", mapNames[i].c_str(), fd.get());
            reuse = true;
        } else {
            fd.reset(bpf_create_map(md[i].type, mapNames[i].c_str(), md[i].key_size, md[i].value_size,
                                    md[i].max_entries, md[i].map_flags));
            ALOGD("bpf_create_map name %s, ret: %d\n", mapNames[i].c_str(), fd.get());
        }

        if (fd < 0) return fd;
        if (fd == 0) return -EINVAL;

        if (!reuse) {
            ret = bpf_obj_pin(fd, mapPinLoc.c_str());
            if (ret) return -errno;
            ret = chown(mapPinLoc.c_str(), (uid_t)md[i].uid, (gid_t)md[i].gid);
            if (ret) return -errno;
            ret = chmod(mapPinLoc.c_str(), md[i].mode);
            if (ret) return -errno;
        }

        mapFds.push_back(std::move(fd));
    }

    return ret;
}

/* For debugging, dump all instructions */
static void dumpIns(char* ins, int size) {
    for (int row = 0; row < size / 8; row++) {
        ALOGE("%d: ", row);
        for (int j = 0; j < 8; j++) {
            ALOGE("%3x ", ins[(row * 8) + j]);
        }
        ALOGE("\n");
    }
}

/* For debugging, dump all code sections from cs list */
static void dumpAllCs(vector<codeSection>& cs) {
    for (int i = 0; i < (int)cs.size(); i++) {
        ALOGE("Dumping cs %d, name %s\n", int(i), cs[i].name.c_str());
        dumpIns((char*)cs[i].data.data(), cs[i].data.size());
        ALOGE("-----------\n");
    }
}

static void applyRelo(void* insnsPtr, Elf64_Addr offset, int fd) {
    int insnIndex;
    struct bpf_insn *insn, *insns;

    insns = (struct bpf_insn*)(insnsPtr);

    insnIndex = offset / sizeof(struct bpf_insn);
    insn = &insns[insnIndex];

    ALOGD(
        "applying relo to instruction at byte offset: %d, \
	       insn offset %d , insn %lx\n",
        (int)offset, (int)insnIndex, *(unsigned long*)insn);

    if (insn->code != (BPF_LD | BPF_IMM | BPF_DW)) {
        ALOGE("Dumping all instructions till ins %d\n", insnIndex);
        ALOGE("invalid relo for insn %d: code 0x%x\n", insnIndex, insn->code);
        dumpIns((char*)insnsPtr, (insnIndex + 3) * 8);
        return;
    }

    insn->imm = fd;
    insn->src_reg = BPF_PSEUDO_MAP_FD;
}

static void applyMapRelo(ifstream& elfFile, vector<unique_fd> &mapFds, vector<codeSection>& cs) {
    vector<string> mapNames;

    int ret = getSectionSymNames(elfFile, "maps", mapNames);
    if (ret) return;

    for (int k = 0; k != (int)cs.size(); k++) {
        Elf64_Rel* rel = (Elf64_Rel*)(cs[k].rel_data.data());
        int n_rel = cs[k].rel_data.size() / sizeof(*rel);

        for (int i = 0; i < n_rel; i++) {
            int symIndex = ELF64_R_SYM(rel[i].r_info);
            string symName;

            ret = getSymNameByIdx(elfFile, symIndex, symName);
            if (ret) return;

            /* Find the map fd and apply relo */
            for (int j = 0; j < (int)mapNames.size(); j++) {
                if (!mapNames[j].compare(symName)) {
                    applyRelo(cs[k].data.data(), rel[i].r_offset, mapFds[j]);
                    break;
                }
            }
        }
    }
}

static int loadCodeSections(const char* elfPath, vector<codeSection>& cs, const string& license) {
    unsigned kvers = kernelVersion();
    int ret, fd;

    if (!kvers) return -1;

    string fname = pathToFilename(string(elfPath), true);

    for (int i = 0; i < (int)cs.size(); i++) {
        string name = cs[i].name;

        if (cs[i].prog_def.has_value()) {
            unsigned min_kver = cs[i].prog_def->min_kver;
            unsigned max_kver = cs[i].prog_def->max_kver;
            ALOGD("cs[%d].name:%s min_kver:%x .max_kver:%x (kvers:%x)\n", i, name.c_str(), min_kver,
                  max_kver, kvers);
            if (kvers < min_kver) continue;
            if (kvers >= max_kver) continue;
        }

        // strip any potential $foo suffix
        // this can be used to provide duplicate programs
        // conditionally loaded based on running kernel version
        name = name.substr(0, name.find_last_of('$'));

        bool reuse = false;
        // Format of pin location is
        // /sys/fs/bpf/prog_<filename>_<mapname>
        string progPinLoc = BPF_FS_PATH "prog_";
        progPinLoc += fname;
        progPinLoc += '_';
        progPinLoc += name;
        if (access(progPinLoc.c_str(), F_OK) == 0) {
            fd = retrieveProgram(progPinLoc.c_str());
            ALOGD("New bpf prog load reusing prog %s, ret: %d (%s)\n", progPinLoc.c_str(), fd,
                  (fd < 0 ? std::strerror(errno) : "no error"));
            reuse = true;
        } else {
            vector<char> log_buf(BPF_LOAD_LOG_SZ, 0);

            fd = bpf_prog_load(cs[i].type, name.c_str(), (struct bpf_insn*)cs[i].data.data(),
                               cs[i].data.size(), license.c_str(), kvers, 0, log_buf.data(),
                               log_buf.size());
            ALOGD("bpf_prog_load lib call for %s (%s) returned fd: %d (%s)\n", elfPath,
                  cs[i].name.c_str(), fd, (fd < 0 ? std::strerror(errno) : "no error"));

            if (fd < 0) {
                std::vector<std::string> lines = android::base::Split(log_buf.data(), "\n");

                ALOGW("bpf_prog_load - BEGIN log_buf contents:");
                for (const auto& line : lines) ALOGW("%s", line.c_str());
                ALOGW("bpf_prog_load - END log_buf contents.");

                if (cs[i].prog_def->optional) {
                    ALOGW("failed program is marked optional - continuing...");
                    continue;
                }
                ALOGE("non-optional program failed to load.");
            }
        }

        if (fd < 0) return fd;
        if (fd == 0) return -EINVAL;

        if (!reuse) {
            ret = bpf_obj_pin(fd, progPinLoc.c_str());
            if (ret) return -errno;
            if (cs[i].prog_def.has_value()) {
                if (chown(progPinLoc.c_str(), (uid_t)cs[i].prog_def->uid,
                          (gid_t)cs[i].prog_def->gid)) {
                    return -errno;
                }
            }
            if (chmod(progPinLoc.c_str(), 0440)) return -errno;
        }

        cs[i].prog_fd.reset(fd);
    }

    return 0;
}

int loadProg(const char* elfPath, bool* isCritical) {
    vector<char> license;
    vector<char> critical;
    vector<codeSection> cs;
    vector<unique_fd> mapFds;
    int ret;

    if (!isCritical) return -1;
    *isCritical = false;

    ifstream elfFile(elfPath, ios::in | ios::binary);
    if (!elfFile.is_open()) return -1;

    ret = readSectionByName("critical", elfFile, critical);
    *isCritical = !ret;

    ret = readSectionByName("license", elfFile, license);
    if (ret) {
        ALOGE("Couldn't find license in %s\n", elfPath);
        return ret;
    } else {
        ALOGD("Loading %s%s ELF object %s with license %s\n",
              *isCritical ? "critical for " : "optional", *isCritical ? (char*)critical.data() : "",
              elfPath, (char*)license.data());
    }

    ret = readCodeSections(elfFile, cs);
    if (ret) {
        ALOGE("Couldn't read all code sections in %s\n", elfPath);
        return ret;
    }

    /* Just for future debugging */
    if (0) dumpAllCs(cs);

    ret = createMaps(elfPath, elfFile, mapFds);
    if (ret) {
        ALOGE("Failed to create maps: (ret=%d) in %s\n", ret, elfPath);
        return ret;
    }

    for (int i = 0; i < (int)mapFds.size(); i++)
        ALOGD("map_fd found at %d is %d in %s\n", i, mapFds[i].get(), elfPath);

    applyMapRelo(elfFile, mapFds, cs);

    ret = loadCodeSections(elfPath, cs, string(license.data()));
    if (ret) ALOGE("Failed to load programs, loadCodeSections ret=%d\n", ret);

    return ret;
}

static bool waitSecondsForProgsLoaded(int seconds) {
    bool ok =
            android::base::WaitForProperty("bpf.progs_loaded", "1", std::chrono::seconds(seconds));
    if (!ok) ALOGW("Waited %ds for bpf.progs_loaded, still waiting...", seconds);
    return ok;
}

void waitForProgsLoaded() {
    if (!android::bpf::isBpfSupported()) return;

    if (waitSecondsForProgsLoaded(5)) return;
    if (waitSecondsForProgsLoaded(10)) return;
    if (waitSecondsForProgsLoaded(20)) return;
    while (!waitSecondsForProgsLoaded(60))
        ;  // loop until success
}

}  // namespace bpf
}  // namespace android
