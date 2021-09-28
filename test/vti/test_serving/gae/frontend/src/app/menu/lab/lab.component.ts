/**
 * Copyright (C) 2018 The Android Open Source Project
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
import { Component, OnInit } from '@angular/core';
import { MatSnackBar, MatTableDataSource, PageEvent } from '@angular/material';

import { AppService } from '../../appservice';
import { Host } from '../../model/host';
import { Lab } from '../../model/lab';
import { LabService } from './lab.service';
import { MenuBaseClass } from '../menu_base';

/** Component that handles lab and host menu. */
@Component({
  selector: 'app-lab',
  templateUrl: './lab.component.html',
  providers: [ LabService ],
  styleUrls: ['./lab.component.scss'],
})
export class LabComponent extends MenuBaseClass implements OnInit {
  labColumnTitles = [
    '_index',
    'name',
    'owner',
    'admin',
    'hostCount',
  ];
  hostColumnTitles = [
    '_index',
    'name',
    'hostname',
    'ip',
    'host_equipment',
    'vtslab_version',
  ];
  labCount = -1;
  labPageIndex = 0;

  constructor(private labService: LabService,
              appService: AppService,
              snackBar: MatSnackBar) {
    super(appService, snackBar);
  }

  labDataSource = new MatTableDataSource<Lab>();
  hostDataSource = new MatTableDataSource<Host>();

  ngOnInit(): void {
    // For labs and hosts, it does not use query pagination.
    this.getHosts();
  }

  /** Gets hosts.
   * @param size A number, at most this many results will be returned.
   * @param offset A Number of results to skip.
   */
  getHosts(size = 0, offset = 0) {
    this.loading = true;
    // Labs will not use filter for query.
    const filterJSON = '';
    this.labService.getLabs(size, offset, filterJSON, '', '')
      .subscribe(
        (response) => {
          this.loading = false;
          if (response.labs) {
            this.count = response.labs.length;
            this.hostDataSource.data = response.labs;
            this.setLabs(response.labs);
          }
        },
        (error) => this.showSnackbar(`[${error.status}] ${error.name}`)
      );
  }

  /** Sets labs from given hosts.
   * @param hosts A list of Host instances.
   */
  setLabs(hosts: Host[]) {
    if (hosts == null || hosts.length === 0) { return; }
    const labMap = new Map();
    hosts.forEach(function(host) {
      if (labMap.has(host.name)) {
        labMap.get(host.name).hosts.push(host);
      } else {
        labMap.set(host.name, {name: host.name, owner: host.owner, admin: host.admin, hosts: [host]});
      }
    });
    const labs: Lab[] = [];
    labMap.forEach((value) => labs.push(value));
    this.labDataSource.data = labs;
  }
}
