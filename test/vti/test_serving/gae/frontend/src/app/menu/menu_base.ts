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

/** This class defines and/or implements the common properties and methods
 * used among menus.
 */
import { AppService } from '../appservice';
import { MatSnackBar } from '@angular/material';
import moment from 'moment-timezone';


export abstract class MenuBaseClass {
  count = -1;

  loading = false;
  pageSizeOptions = [20, 50, 100, 200];
  pageSize = 100;
  pageIndex = 0;

  protected constructor(private appService: AppService,
                        public snackBar: MatSnackBar) {
    this.appService.closeSideNav();
    this.snackBar.dismiss();
  }

  /** Returns an Observable which handles a response of count API.
   * @param additionalOperations A list of lambda functions.
   */
  getDefaultCountObservable(additionalOperations: any[] = []) {
    return {
      next: (response) => {
        this.count = response.count;
        for (const operation of additionalOperations) {
          operation(response);
        }
      },
      error: (error) => this.showSnackbar(`[${error.status}] ${error.name}`)
    };
  }

  getRelativeTime(timeString) {
    return (moment.tz(timeString, 'YYYY-MM-DDThh:mm:ss', 'UTC').isValid() ?
      moment.tz(timeString, 'YYYY-MM-DDThh:mm:ss', 'UTC').fromNow() : '---');
  }

  /** Checks whether timeString is expired from current time. */
  isExpired(timeString, hours=72) {
    let currentTime = moment.tz(timeString, 'YYYY-MM-DDThh:mm:ss', 'UTC');
    if (!currentTime.isValid()) { return false; }

    let diff = moment().diff(currentTime);
    let duration = moment.duration(diff);
    return duration.asHours() > hours;
  }

  /** Displays a snackbar notification. */
  showSnackbar(message = 'Error', duration = 5000) {
    this.loading = false;
    this.snackBar.open(message, 'DISMISS', {duration});
  }

  /** Displays a side nav window and lists all properties of selected entity. */
  onShowDetailsClicked(entity) {
    this.appService.showDetails(entity);
  }
}
