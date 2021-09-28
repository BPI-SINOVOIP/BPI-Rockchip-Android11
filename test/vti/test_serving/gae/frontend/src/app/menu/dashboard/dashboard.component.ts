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
import { MatSnackBar } from '@angular/material';

import { AppService } from '../../appservice';
import { BuildService } from "../build/build.service";
import { MenuBaseClass } from "../menu_base";
import { ScheduleService } from "../schedule/schedule.service";

/** Component that handles dashboard. */
@Component({
  selector: 'app-dashboard',
  templateUrl: './dashboard.component.html',
  providers: [ BuildService, ScheduleService ],
  styleUrls: ['./dashboard.component.scss']
})
export class DashboardComponent extends MenuBaseClass implements OnInit {
  lastBuildUpdateTime: any = '---';
  lastScheduleUpdateTime: any = '---';

  constructor(private buildService: BuildService,
              private scheduleService: ScheduleService,
              appService: AppService,
              snackBar: MatSnackBar) {
    super(appService, snackBar);
  }

  ngOnInit(): void {
    this.getLatestBuild();
    this.getLastestSchedule();
  }

  /** Fetches the most recently updated build and gets timestamp from it. */
  getLatestBuild() {
    this.lastBuildUpdateTime = '---';
    this.buildService.getBuilds(1, 0, '', 'timestamp', 'desc')
      .subscribe(
        (response) => {
          if (response.builds) {
            this.lastBuildUpdateTime = response.builds[0].timestamp;
          }
        },
        (error) => this.showSnackbar(`[${error.status}] ${error.name}`)
      );
  }

  /** Fetches the most recently updated schedule and gets timestamp from it. */
  getLastestSchedule() {
    this.lastScheduleUpdateTime = '---';
    this.scheduleService.getSchedules(1, 0, '', 'timestamp', 'desc')
      .subscribe(
        (response) => {
          if (response.schedules) {
            this.lastScheduleUpdateTime = response.schedules[0].timestamp;
          }
        },
        (error) => this.showSnackbar(`[${error.status}] ${error.name}`)
      );
  }
}
