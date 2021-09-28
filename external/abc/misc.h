/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __MISC_H__
#define __MISC_H__

extern char latest_file[PATH_MAX];

extern void create_log_directory(const char* sd_path);
extern void copy_all_logs_to_storage(const char* path);

extern void init_all(void);
extern void delete_dir(const char *);
int base64_encode(const char *, char *, const unsigned long);

extern char latest_log_path[PATH_MAX];
extern char new_log_path[PATH_MAX];
extern int trigger_upload;

#endif
