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
import { Component, OnInit, ViewChild } from '@angular/core';
import { MatSnackBar, MatTableDataSource, PageEvent } from '@angular/material';
import { animate, state, style, transition, trigger } from "@angular/animations";

import { AppService } from '../../appservice';
import { FilterComponent } from '../../shared/filter/filter.component';
import { FilterItem } from '../../model/filter_item';
import { MenuBaseClass } from '../menu_base';
import { Schedule, ScheduleSuspendResponse } from '../../model/schedule';
import { ScheduleService } from './schedule.service';


/** Component that handles schedule menu. */
@Component({
  selector: 'app-schedule',
  templateUrl: './schedule.component.html',
  providers: [ ScheduleService ],
  styleUrls: ['./schedule.component.scss'],
  animations: [
    trigger('detailExpand', [
      state('void', style({height: '0px', minHeight: '0', visibility: 'hidden'})),
      state('*', style({height: '*', visibility: 'visible'})),
      transition('void <=> *', animate('225ms cubic-bezier(0.4, 0.0, 0.2, 1)')),
    ]),
  ],
})
export class ScheduleComponent extends MenuBaseClass implements OnInit {
  columnTitles = [
    '_index',
    'test_name',
    'device',
    'manifest_branch',
    'build_target',
    'gsi_branch',
    'gsi_build_target',
    'test_branch',
    'test_build_target',
    'period',
    'status',
    'timestamp',
  ];
  dataSource = new MatTableDataSource<Schedule>();
  pageEvent: PageEvent;
  appliedFilters: FilterItem[];

  @ViewChild(FilterComponent) filterComponent: FilterComponent;

  constructor(private scheduleService: ScheduleService,
              appService: AppService,
              snackBar: MatSnackBar) {
    super(appService, snackBar);
  }

  ngOnInit(): void {
    this.filterComponent.setSelectorList(Schedule);
    this.getCount();
    this.getSchedules(this.pageSize, this.pageSize * this.pageIndex);
  }

  /** Gets a total count of schedules. */
  getCount(observer = this.getDefaultCountObservable()) {
    const filterJSON = (this.appliedFilters) ? JSON.stringify(this.appliedFilters) : '';
    this.scheduleService.getCount(filterJSON).subscribe(observer);
  }

  /** Gets schedules.
   * @param size A number, at most this many results will be returned.
   * @param offset A Number of results to skip.
   */
  getSchedules(size = 0, offset = 0) {
    this.loading = true;
    const filterJSON = (this.appliedFilters) ? JSON.stringify(this.appliedFilters) : '';
    this.scheduleService.getSchedules(size, offset, filterJSON, '', '')
      .subscribe(
        (response) => {
          this.loading = false;
          if (this.count >= 0) {
            let length = 0;
            if (response.schedules) {
              length = response.schedules.length;
            }
            const total = length + offset;
            if (response.has_next) {
              if (length !== this.pageSize) {
                this.showSnackbar('Received unexpected number of entities.');
              } else if (this.count <= total) {
                this.getCount();
              }
            } else {
              if (this.count !== total) {
                if (length !== this.count) {
                  this.getCount();
                } else if (this.count > total) {
                  const countObservable = this.getDefaultCountObservable([
                    () => {
                      this.pageIndex = Math.floor(this.count / this.pageSize);
                      this.getSchedules(this.pageSize, this.pageSize * this.pageIndex);
                    }
                  ]);
                  this.getCount(countObservable);
                }
              }
            }
          }
          this.dataSource.data = response.schedules;
        },
        (error) => this.showSnackbar(`[${error.status}] ${error.name}`)
      );
  }

  /** Toggles a schedule from suspend to resume, or vice versa. */
  suspendSchedule(schedules: ScheduleSuspendResponse[]) {
    this.scheduleService.suspendSchedule(schedules)
      .subscribe(
        (response) => {
          if (response.schedules) {
            let self = this;
            response.schedules.forEach(function(schedule) {
                const original = self.dataSource.data.filter(x => x.urlsafe_key === schedule.urlsafe_key);
                if (original) {
                  original[0].suspended = schedule.suspend;
                }
              })
          }
        },
        (error) => this.showSnackbar(`[${error.status}] ${error.name}`)
      );
  }

  /** Hooks a page event and handles properly. */
  onPageEvent(event: PageEvent) {
    this.pageSize = event.pageSize;
    this.pageIndex = event.pageIndex;
    this.getSchedules(this.pageSize, this.pageSize * this.pageIndex);
    return event;
  }

  /** Applies a filter and get entities with it. */
  applyFilters(filters) {
    this.pageIndex = 0;
    this.appliedFilters = filters;
    this.getCount();
    this.getSchedules(this.pageSize, this.pageSize * this.pageIndex);
  }
}
