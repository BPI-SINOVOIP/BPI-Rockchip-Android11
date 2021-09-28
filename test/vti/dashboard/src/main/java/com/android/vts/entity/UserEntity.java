/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License. You may
 * obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

package com.android.vts.entity;

import com.googlecode.objectify.Key;
import com.googlecode.objectify.Ref;
import com.googlecode.objectify.annotation.Cache;
import com.googlecode.objectify.annotation.Entity;
import com.googlecode.objectify.annotation.Id;
import com.googlecode.objectify.annotation.Index;
import com.googlecode.objectify.annotation.Load;
import java.util.ArrayList;
import java.util.List;
import lombok.EqualsAndHashCode;
import lombok.Getter;
import lombok.NoArgsConstructor;
import lombok.Setter;
import java.util.Date;

import static com.googlecode.objectify.ObjectifyService.ofy;

/** Entity Class for User */
@Cache
@Entity
@EqualsAndHashCode(of = "email")
@NoArgsConstructor
public class UserEntity implements DashboardEntity {

    /** User email field */
    @Id @Getter @Setter String email;

    /** User name field */
    @Getter @Setter String name;

    /** User name field */
    @Getter @Setter String company;

    /** User enable or disable field */
    @Index @Getter @Setter Boolean enable;

    /** User admin flag field */
    @Index @Getter @Setter Boolean isAdmin;

    /** When this record was created or updated */
    @Index @Getter Date updated;

    @Load
    @Getter
    List<Ref<RoleEntity>> roles = new ArrayList<>();

    /** Constructor function for UserEntity Class */
    public UserEntity(
            String email,
            String name,
            String company,
            String roleName) {

        this.email = email;
        this.name = name;
        this.enable = true;
        this.isAdmin = false;
        this.company = company;
        RoleEntity roleEntity = RoleEntity.getRole(roleName);
        this.roles.add(Ref.create(roleEntity));
    }

    /** Saving function for the instance of this class */
    @Override
    public Key<UserEntity> save() {
        this.updated = new Date();
        return ofy().save().entity(this).now();
    }

    /** Get admin user list by admin email */
    public static UserEntity getAdminUser(String adminEmail) {
        Key key = Key.create(UserEntity.class, adminEmail);
        return ofy().load()
            .type(UserEntity.class)
            .filterKey(key)
            .filter("enable", true)
            .filter("isAdmin", true)
            .first()
            .now();
    }

    /** Get all admin user list */
    public static List<UserEntity> getAdminUserList() {
        return ofy().load()
            .type(UserEntity.class)
            .filter("enable", true)
            .filter("isAdmin", true)
            .list();
    }

    /** Get normal user list */
    public static List<UserEntity> getUserList() {
        return ofy().load()
            .type(UserEntity.class)
            .filter("enable", true)
            .filter("isAdmin", false)
            .list();
    }

    /** Get user by email */
    public static UserEntity getUser(String email) {
        Key key = Key.create(UserEntity.class, email);
        return ofy().load()
            .type(UserEntity.class)
            .filterKey(key)
            .filter("enable", true)
            .first()
            .now();
    }
}
