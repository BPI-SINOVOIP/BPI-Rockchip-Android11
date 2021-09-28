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
import { Component, EventEmitter, Input, OnInit, Output } from '@angular/core';
import { FilterItem } from '../../model/filter_item';
import { FilterCondition } from '../../model/filter_condition';

@Component({
  selector: 'app-filter',
  templateUrl: './filter.component.html',
  styleUrls: ['./filter.component.scss']
})
export class FilterComponent implements OnInit {
  currentFilter: FilterItem;
  applyingFilters: FilterItem[] = [];
  applyingFilterChanged = false;
  appliedFilters: FilterItem[] = [];
  selectorList: string[];

  filterMethods = [
    {value: FilterCondition.EqualTo, text: 'is equal to', sign: '='},
    {value: FilterCondition.LessThan, text: 'is less than', sign: '<'},
    {value: FilterCondition.GreaterThan, text: 'is greater than', sign: '>'},
    {value: FilterCondition.LessThanOrEqualTo, text: 'is less than or equal to', sign: '<='},
    {value: FilterCondition.GreaterThanOrEqualTo, text: 'is greater than or equal to', sign: '>='},
    {value: FilterCondition.NotEqualTo, text: 'is not equal to', sign: '!='},
    {value: FilterCondition.Has, text: 'has', sign: 'has'},
  ];

  @Output() applyFilters = new EventEmitter();
  @Input() disabled: boolean;

  panelOpenState = false;

  ngOnInit(): void {
    this.currentFilter = new FilterItem();
    this.currentFilter.value = '';
  }

  /** Sets a filter key list with the given class. */
  setSelectorList(typeOfClass: any) {
    const instance = new typeOfClass();
    this.selectorList = Object.getOwnPropertyNames(instance);
  }

  /** Adds the current filter to the list of filters to be applied. */
  addFilter() {
    this.applyingFilters.push(this.currentFilter);
    this.currentFilter = new FilterItem();
    this.currentFilter.value = '';
    this.applyingFilterChanged = true;
  }

  /** Clears the current filter. */
  clearCurrentFilter() {
    this.currentFilter.key = undefined;
    this.currentFilter.method = undefined;
    this.currentFilter.value = '';
  }

  /** Removes the selected filter from the list of filters to be applied. */
  removed(filter: FilterItem) {
    const index = this.applyingFilters.indexOf(filter);
    if (index >= 0) {
      this.applyingFilters.splice(index, 1);
      this.applyingFilterChanged = true;
    }
  }

  /** Gets a filter sign with method value. */
  getSign(filter: FilterItem) {
    return this.filterMethods.find((x) => x.value === filter.method).sign;
  }

  /** Applies the list of filters. */
  onApplyClicked() {
    this.applyFilters.emit(this.applyingFilters);
    this.appliedFilters = this.applyingFilters.slice();
    this.applyingFilterChanged = false;
  }

  /** Cancels the current changes and roll back to the last applied filters. */
  onCancelChangesClicked() {
    this.applyingFilters = this.appliedFilters.slice();
    this.applyingFilterChanged = false;
  }

  /** Reset all filters. */
  onClearAllClicked() {
    this.applyingFilters = [];
    this.appliedFilters = [];
    this.applyFilters.emit(this.appliedFilters);
    this.applyingFilterChanged = false;
  }
}
