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

import { AppService } from '../../appservice';
import { Device } from '../../model/device';
import { DeviceService } from './device.service';
import { DeviceStatus, SchedulingStatus } from '../../shared/vtslab_status';
import { FilterComponent } from '../../shared/filter/filter.component';
import { FilterItem } from '../../model/filter_item';
import { MenuBaseClass } from '../menu_base';


/** Component that handles device menu. */
@Component({
  selector: 'app-device',
  templateUrl: './device.component.html',
  providers: [ DeviceService ],
  styleUrls: ['./device.component.scss'],
})
export class DeviceComponent extends MenuBaseClass implements OnInit {
  columnTitles = [
    '_index',
    'hostname',
    'product',
    'serial',
    'status',
    'scheduling_status',
    'device_equipment',
    'timestamp',
  ];
  dataSource = new MatTableDataSource<Device>();
  pageEvent: PageEvent;
  deviceStatusEnum = DeviceStatus;
  schedulingStatusEnum = SchedulingStatus;
  appliedFilters: FilterItem[];

  sort = '';
  sortDirection = '';

  @ViewChild(FilterComponent) filterComponent: FilterComponent;

  constructor(private deviceService: DeviceService,
              appService: AppService,
              snackBar: MatSnackBar) {
    super(appService, snackBar);
  }

  ngOnInit(): void {
    this.sort = 'hostname';
    this.sortDirection = 'asc';

    this.filterComponent.setSelectorList(Device);
    this.getCount();
    this.getDevices(this.pageSize, this.pageSize * this.pageIndex);
  }

  /** Gets a total count of devices. */
  getCount(observer = this.getDefaultCountObservable()) {
    const filterJSON = (this.appliedFilters) ? JSON.stringify(this.appliedFilters) : '';
    this.deviceService.getCount(filterJSON).subscribe(observer);
  }

  /** Gets devices.
   * @param size A number, at most this many results will be returned.
   * @param offset A Number of results to skip.
   */
  getDevices(size = 0, offset = 0) {
    this.loading = true;
    const filterJSON = (this.appliedFilters) ? JSON.stringify(this.appliedFilters) : '';
    this.deviceService.getDevices(size, offset, filterJSON, this.sort, this.sortDirection)
      .subscribe(
        (response) => {
          this.loading = false;
          if (this.count >= 0) {
            let length = 0;
            if (response.devices) {
              length = response.devices.length;
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
                      this.getDevices(this.pageSize, this.pageSize * this.pageIndex);
                    }
                  ]);
                  this.getCount(countObservable);
                }
              }
            }
          }
          this.dataSource.data = response.devices;
        },
        (error) => this.showSnackbar(`[${error.status}] ${error.name}`)
      );
  }

  /** Hooks a page event and handles properly. */
  onPageEvent(event: PageEvent) {
    this.pageSize = event.pageSize;
    this.pageIndex = event.pageIndex;
    this.getDevices(this.pageSize, this.pageSize * this.pageIndex);
    return event;
  }

  /** Applies a filter and get entities with it. */
  applyFilters(filters) {
    this.pageIndex = 0;
    this.appliedFilters = filters;
    this.getCount();
    this.getDevices(this.pageSize, this.pageSize * this.pageIndex);
  }
}
