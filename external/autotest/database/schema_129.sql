-- MySQL dump 10.14  Distrib 5.5.32-MariaDB, for Linux ()
--
-- Host: localhost    Database: chromeos_autotest_db
-- ------------------------------------------------------
-- Server version	5.5.32-MariaDB

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8 */;
/*!40103 SET @OLD_TIME_ZONE=@@TIME_ZONE */;
/*!40103 SET TIME_ZONE='+00:00' */;
/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;

--
-- Table structure for table `afe_aborted_host_queue_entries`
--

DROP TABLE IF EXISTS `afe_aborted_host_queue_entries`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `afe_aborted_host_queue_entries` (
  `queue_entry_id` int(11) NOT NULL,
  `aborted_by_id` int(11) NOT NULL,
  `aborted_on` datetime NOT NULL,
  PRIMARY KEY (`queue_entry_id`),
  KEY `aborted_host_queue_entries_aborted_by_id_fk` (`aborted_by_id`),
  CONSTRAINT `aborted_host_queue_entries_aborted_by_id_fk` FOREIGN KEY (`aborted_by_id`) REFERENCES `afe_users` (`id`) ON DELETE NO ACTION,
  CONSTRAINT `aborted_host_queue_entries_queue_entry_id_fk` FOREIGN KEY (`queue_entry_id`) REFERENCES `afe_host_queue_entries` (`id`) ON DELETE NO ACTION
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `afe_acl_groups`
--

DROP TABLE IF EXISTS `afe_acl_groups`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `afe_acl_groups` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `name` varchar(255) DEFAULT NULL,
  `description` varchar(255) DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=2 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `afe_acl_groups_hosts`
--

DROP TABLE IF EXISTS `afe_acl_groups_hosts`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `afe_acl_groups_hosts` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `aclgroup_id` int(11) DEFAULT NULL,
  `host_id` int(11) DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `acl_groups_hosts_both_ids` (`aclgroup_id`,`host_id`),
  KEY `acl_groups_hosts_host_id` (`host_id`),
  CONSTRAINT `acl_groups_hosts_aclgroup_id_fk` FOREIGN KEY (`aclgroup_id`) REFERENCES `afe_acl_groups` (`id`) ON DELETE NO ACTION,
  CONSTRAINT `acl_groups_hosts_host_id_fk` FOREIGN KEY (`host_id`) REFERENCES `afe_hosts` (`id`) ON DELETE NO ACTION
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `afe_acl_groups_users`
--

DROP TABLE IF EXISTS `afe_acl_groups_users`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `afe_acl_groups_users` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `aclgroup_id` int(11) DEFAULT NULL,
  `user_id` int(11) DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `acl_groups_users_both_ids` (`aclgroup_id`,`user_id`),
  KEY `acl_groups_users_user_id` (`user_id`),
  CONSTRAINT `acl_groups_users_aclgroup_id_fk` FOREIGN KEY (`aclgroup_id`) REFERENCES `afe_acl_groups` (`id`) ON DELETE NO ACTION,
  CONSTRAINT `acl_groups_users_user_id_fk` FOREIGN KEY (`user_id`) REFERENCES `afe_users` (`id`) ON DELETE NO ACTION
) ENGINE=InnoDB AUTO_INCREMENT=2 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `afe_atomic_groups`
--

DROP TABLE IF EXISTS `afe_atomic_groups`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `afe_atomic_groups` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `name` varchar(255) NOT NULL,
  `description` longtext,
  `max_number_of_machines` int(11) NOT NULL,
  `invalid` tinyint(1) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `name` (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `afe_autotests`
--

DROP TABLE IF EXISTS `afe_autotests`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `afe_autotests` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `name` varchar(255) DEFAULT NULL,
  `test_class` varchar(255) DEFAULT NULL,
  `description` text,
  `test_type` int(11) DEFAULT NULL,
  `path` varchar(255) DEFAULT NULL,
  `author` varchar(256) DEFAULT NULL,
  `dependencies` varchar(256) DEFAULT NULL,
  `experimental` smallint(6) DEFAULT '0',
  `run_verify` smallint(6) DEFAULT '1',
  `test_time` smallint(6) DEFAULT '1',
  `test_category` varchar(256) DEFAULT NULL,
  `sync_count` int(11) DEFAULT '1',
  `test_retry` int(11) NOT NULL DEFAULT '0',
  `run_reset` smallint(6) NOT NULL DEFAULT '1',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=2857 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `afe_autotests_dependency_labels`
--

DROP TABLE IF EXISTS `afe_autotests_dependency_labels`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `afe_autotests_dependency_labels` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `test_id` int(11) NOT NULL,
  `label_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `test_id` (`test_id`,`label_id`),
  KEY `autotests_dependency_labels_label_id_fk` (`label_id`),
  CONSTRAINT `autotests_dependency_labels_label_id_fk` FOREIGN KEY (`label_id`) REFERENCES `afe_labels` (`id`) ON DELETE NO ACTION,
  CONSTRAINT `autotests_dependency_labels_test_id_fk` FOREIGN KEY (`test_id`) REFERENCES `afe_autotests` (`id`) ON DELETE NO ACTION
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `afe_drone_sets`
--

DROP TABLE IF EXISTS `afe_drone_sets`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `afe_drone_sets` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `name` varchar(255) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `afe_drone_sets_unique` (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `afe_drone_sets_drones`
--

DROP TABLE IF EXISTS `afe_drone_sets_drones`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `afe_drone_sets_drones` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `droneset_id` int(11) NOT NULL,
  `drone_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `afe_drone_sets_drones_unique` (`drone_id`),
  KEY `afe_drone_sets_drones_droneset_ibfk` (`droneset_id`),
  CONSTRAINT `afe_drone_sets_drones_drone_ibfk` FOREIGN KEY (`drone_id`) REFERENCES `afe_drones` (`id`),
  CONSTRAINT `afe_drone_sets_drones_droneset_ibfk` FOREIGN KEY (`droneset_id`) REFERENCES `afe_drone_sets` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `afe_drones`
--

DROP TABLE IF EXISTS `afe_drones`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `afe_drones` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `hostname` varchar(255) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `afe_drones_unique` (`hostname`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `afe_host_attributes`
--

DROP TABLE IF EXISTS `afe_host_attributes`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `afe_host_attributes` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `host_id` int(11) NOT NULL,
  `attribute` varchar(90) NOT NULL,
  `value` varchar(300) NOT NULL,
  PRIMARY KEY (`id`),
  KEY `host_id` (`host_id`),
  KEY `attribute` (`attribute`),
  CONSTRAINT `afe_host_attributes_ibfk_1` FOREIGN KEY (`host_id`) REFERENCES `afe_hosts` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `afe_host_queue_entries`
--

DROP TABLE IF EXISTS `afe_host_queue_entries`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `afe_host_queue_entries` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `job_id` int(11) DEFAULT NULL,
  `host_id` int(11) DEFAULT NULL,
  `status` varchar(255) DEFAULT NULL,
  `meta_host` int(11) DEFAULT NULL,
  `active` tinyint(1) DEFAULT '0',
  `complete` tinyint(1) DEFAULT '0',
  `deleted` tinyint(1) NOT NULL,
  `execution_subdir` varchar(255) NOT NULL,
  `atomic_group_id` int(11) DEFAULT NULL,
  `aborted` tinyint(1) NOT NULL DEFAULT '0',
  `started_on` datetime DEFAULT NULL,
  `finished_on` datetime DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `host_queue_entries_job_id_and_host_id` (`job_id`,`host_id`),
  KEY `host_queue_entries_host_id` (`host_id`),
  KEY `host_queue_entries_meta_host` (`meta_host`),
  KEY `atomic_group_id` (`atomic_group_id`),
  KEY `host_queue_entries_host_active` (`host_id`,`active`),
  KEY `host_queue_entry_status` (`status`),
  KEY `host_queue_entries_abort_incomplete` (`aborted`,`complete`),
  KEY `afe_host_queue_entries_active` (`active`),
  KEY `afe_host_queue_entries_complete` (`complete`),
  KEY `afe_host_queue_entries_deleted` (`deleted`),
  KEY `afe_host_queue_entries_aborted` (`aborted`),
  KEY `afe_host_queue_entries_started_on` (`started_on`),
  KEY `afe_host_queue_entries_finished_on` (`finished_on`),
  KEY `afe_host_queue_entries_job_id` (`job_id`),
  CONSTRAINT `afe_host_queue_entries_ibfk_1` FOREIGN KEY (`atomic_group_id`) REFERENCES `afe_atomic_groups` (`id`) ON DELETE NO ACTION,
  CONSTRAINT `host_queue_entries_host_id_fk` FOREIGN KEY (`host_id`) REFERENCES `afe_hosts` (`id`) ON DELETE NO ACTION,
  CONSTRAINT `host_queue_entries_job_id_fk` FOREIGN KEY (`job_id`) REFERENCES `afe_jobs` (`id`) ON DELETE NO ACTION,
  CONSTRAINT `host_queue_entries_meta_host_fk` FOREIGN KEY (`meta_host`) REFERENCES `afe_labels` (`id`) ON DELETE NO ACTION
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `afe_host_queue_entry_start_times`
--

DROP TABLE IF EXISTS `afe_host_queue_entry_start_times`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `afe_host_queue_entry_start_times` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `insert_time` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  `highest_hqe_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  KEY `afe_hqe_insert_times_index` (`insert_time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `afe_hosts`
--

DROP TABLE IF EXISTS `afe_hosts`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `afe_hosts` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `hostname` varchar(255) DEFAULT NULL,
  `locked` tinyint(1) DEFAULT '0',
  `synch_id` int(11) DEFAULT NULL,
  `status` varchar(255) DEFAULT NULL,
  `invalid` tinyint(1) DEFAULT '0',
  `protection` int(11) NOT NULL,
  `locked_by_id` int(11) DEFAULT NULL,
  `lock_time` datetime DEFAULT NULL,
  `dirty` tinyint(1) NOT NULL,
  `leased` tinyint(1) NOT NULL DEFAULT '1',
  `shard_id` int(11) DEFAULT NULL,
  `lock_reason` text,
  PRIMARY KEY (`id`),
  KEY `hosts_locked_by_fk` (`locked_by_id`),
  KEY `leased_hosts` (`leased`,`locked`),
  KEY `hosts_to_shard_ibfk` (`shard_id`),
  CONSTRAINT `hosts_locked_by_fk` FOREIGN KEY (`locked_by_id`) REFERENCES `afe_users` (`id`) ON DELETE NO ACTION,
  CONSTRAINT `hosts_to_shard_ibfk` FOREIGN KEY (`shard_id`) REFERENCES `afe_shards` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `afe_hosts_labels`
--

DROP TABLE IF EXISTS `afe_hosts_labels`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `afe_hosts_labels` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `host_id` int(11) DEFAULT NULL,
  `label_id` int(11) DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hosts_labels_both_ids` (`label_id`,`host_id`),
  KEY `hosts_labels_host_id` (`host_id`),
  CONSTRAINT `hosts_labels_host_id_fk` FOREIGN KEY (`host_id`) REFERENCES `afe_hosts` (`id`) ON DELETE NO ACTION,
  CONSTRAINT `hosts_labels_label_id_fk` FOREIGN KEY (`label_id`) REFERENCES `afe_labels` (`id`) ON DELETE NO ACTION
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `afe_ineligible_host_queues`
--

DROP TABLE IF EXISTS `afe_ineligible_host_queues`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `afe_ineligible_host_queues` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `job_id` int(11) DEFAULT NULL,
  `host_id` int(11) DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `ineligible_host_queues_both_ids` (`host_id`,`job_id`),
  KEY `ineligible_host_queues_job_id` (`job_id`),
  CONSTRAINT `ineligible_host_queues_host_id_fk` FOREIGN KEY (`host_id`) REFERENCES `afe_hosts` (`id`) ON DELETE NO ACTION,
  CONSTRAINT `ineligible_host_queues_job_id_fk` FOREIGN KEY (`job_id`) REFERENCES `afe_jobs` (`id`) ON DELETE NO ACTION
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `afe_job_handoffs`
--

DROP TABLE IF EXISTS `afe_job_handoffs`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `afe_job_handoffs` (
  `job_id` int(11) NOT NULL,
  `created` datetime NOT NULL,
  `completed` tinyint(1) NOT NULL,
  `drone` varchar(128) DEFAULT NULL,
  PRIMARY KEY (`job_id`),
  CONSTRAINT `job_fk` FOREIGN KEY (`job_id`) REFERENCES `afe_jobs` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `afe_job_keyvals`
--

DROP TABLE IF EXISTS `afe_job_keyvals`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `afe_job_keyvals` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `job_id` int(11) NOT NULL,
  `key` varchar(90) NOT NULL,
  `value` varchar(300) NOT NULL,
  PRIMARY KEY (`id`),
  KEY `afe_job_keyvals_job_id` (`job_id`),
  KEY `afe_job_keyvals_key` (`key`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `afe_jobs`
--

DROP TABLE IF EXISTS `afe_jobs`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `afe_jobs` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `owner` varchar(255) DEFAULT NULL,
  `name` varchar(255) DEFAULT NULL,
  `priority` int(11) DEFAULT NULL,
  `control_file` text,
  `control_type` int(11) DEFAULT NULL,
  `created_on` datetime DEFAULT NULL,
  `synch_count` int(11) NOT NULL,
  `timeout` int(11) NOT NULL,
  `run_verify` tinyint(1) DEFAULT '1',
  `email_list` varchar(250) NOT NULL,
  `reboot_before` smallint(6) NOT NULL,
  `reboot_after` smallint(6) NOT NULL,
  `parse_failed_repair` tinyint(1) NOT NULL DEFAULT '1',
  `max_runtime_hrs` int(11) NOT NULL,
  `drone_set_id` int(11) DEFAULT NULL,
  `parameterized_job_id` int(11) DEFAULT NULL,
  `max_runtime_mins` int(11) NOT NULL,
  `parent_job_id` int(11) DEFAULT NULL,
  `test_retry` int(11) NOT NULL DEFAULT '0',
  `run_reset` smallint(6) NOT NULL DEFAULT '1',
  `timeout_mins` int(11) NOT NULL,
  `shard_id` int(11) DEFAULT NULL,
  `require_ssp` tinyint(1) DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `afe_jobs_drone_set_ibfk` (`drone_set_id`),
  KEY `afe_jobs_parameterized_job_ibfk` (`parameterized_job_id`),
  KEY `created_on` (`created_on`),
  KEY `parent_job_id_index` (`parent_job_id`),
  KEY `owner_index` (`owner`),
  KEY `jobs_to_shard_ibfk` (`shard_id`),
  KEY `name_index` (`name`),
  CONSTRAINT `afe_jobs_drone_set_ibfk` FOREIGN KEY (`drone_set_id`) REFERENCES `afe_drone_sets` (`id`),
  CONSTRAINT `afe_jobs_parameterized_job_ibfk` FOREIGN KEY (`parameterized_job_id`) REFERENCES `afe_parameterized_jobs` (`id`),
  CONSTRAINT `jobs_to_shard_ibfk` FOREIGN KEY (`shard_id`) REFERENCES `afe_shards` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `afe_jobs_dependency_labels`
--

DROP TABLE IF EXISTS `afe_jobs_dependency_labels`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `afe_jobs_dependency_labels` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `job_id` int(11) NOT NULL,
  `label_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `job_id` (`job_id`,`label_id`),
  KEY `jobs_dependency_labels_label_id_fk` (`label_id`),
  CONSTRAINT `jobs_dependency_labels_job_id_fk` FOREIGN KEY (`job_id`) REFERENCES `afe_jobs` (`id`) ON DELETE NO ACTION,
  CONSTRAINT `jobs_dependency_labels_label_id_fk` FOREIGN KEY (`label_id`) REFERENCES `afe_labels` (`id`) ON DELETE NO ACTION
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `afe_kernels`
--

DROP TABLE IF EXISTS `afe_kernels`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `afe_kernels` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `version` varchar(255) NOT NULL,
  `cmdline` varchar(255) DEFAULT '',
  PRIMARY KEY (`id`),
  UNIQUE KEY `afe_kernals_unique` (`version`,`cmdline`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `afe_labels`
--

DROP TABLE IF EXISTS `afe_labels`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `afe_labels` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `name` varchar(750) DEFAULT NULL,
  `kernel_config` varchar(255) DEFAULT NULL,
  `platform` tinyint(1) DEFAULT '0',
  `invalid` tinyint(1) NOT NULL,
  `only_if_needed` tinyint(1) NOT NULL,
  `atomic_group_id` int(11) DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `name` (`name`),
  KEY `atomic_group_id` (`atomic_group_id`),
  CONSTRAINT `afe_labels_ibfk_1` FOREIGN KEY (`atomic_group_id`) REFERENCES `afe_atomic_groups` (`id`) ON DELETE NO ACTION
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `afe_parameterized_job_parameters`
--

DROP TABLE IF EXISTS `afe_parameterized_job_parameters`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `afe_parameterized_job_parameters` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `parameterized_job_id` int(11) NOT NULL,
  `test_parameter_id` int(11) NOT NULL,
  `parameter_value` text NOT NULL,
  `parameter_type` enum('int','float','string') DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `afe_parameterized_job_parameters_unique` (`parameterized_job_id`,`test_parameter_id`),
  KEY `afe_parameterized_job_parameters_test_parameter_ibfk` (`test_parameter_id`),
  CONSTRAINT `afe_parameterized_job_parameters_test_parameter_ibfk` FOREIGN KEY (`test_parameter_id`) REFERENCES `afe_test_parameters` (`id`),
  CONSTRAINT `afe_parameterized_job_parameters_job_ibfk` FOREIGN KEY (`parameterized_job_id`) REFERENCES `afe_parameterized_jobs` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `afe_parameterized_job_profiler_parameters`
--

DROP TABLE IF EXISTS `afe_parameterized_job_profiler_parameters`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `afe_parameterized_job_profiler_parameters` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `parameterized_job_profiler_id` int(11) NOT NULL,
  `parameter_name` varchar(255) NOT NULL,
  `parameter_value` text NOT NULL,
  `parameter_type` enum('int','float','string') DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `afe_parameterized_job_profiler_parameters_unique` (`parameterized_job_profiler_id`,`parameter_name`),
  CONSTRAINT `afe_parameterized_job_profiler_parameters_ibfk` FOREIGN KEY (`parameterized_job_profiler_id`) REFERENCES `afe_parameterized_jobs_profilers` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `afe_parameterized_jobs`
--

DROP TABLE IF EXISTS `afe_parameterized_jobs`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `afe_parameterized_jobs` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `test_id` int(11) NOT NULL,
  `label_id` int(11) DEFAULT NULL,
  `use_container` tinyint(1) DEFAULT '0',
  `profile_only` tinyint(1) DEFAULT '0',
  `upload_kernel_config` tinyint(1) DEFAULT '0',
  PRIMARY KEY (`id`),
  KEY `afe_parameterized_jobs_test_ibfk` (`test_id`),
  KEY `afe_parameterized_jobs_label_ibfk` (`label_id`),
  CONSTRAINT `afe_parameterized_jobs_label_ibfk` FOREIGN KEY (`label_id`) REFERENCES `afe_labels` (`id`),
  CONSTRAINT `afe_parameterized_jobs_test_ibfk` FOREIGN KEY (`test_id`) REFERENCES `afe_autotests` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `afe_parameterized_jobs_kernels`
--

DROP TABLE IF EXISTS `afe_parameterized_jobs_kernels`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `afe_parameterized_jobs_kernels` (
  `parameterized_job_id` int(11) NOT NULL,
  `kernel_id` int(11) NOT NULL,
  PRIMARY KEY (`parameterized_job_id`,`kernel_id`),
  CONSTRAINT `afe_parameterized_jobs_kernels_parameterized_job_ibfk` FOREIGN KEY (`parameterized_job_id`) REFERENCES `afe_parameterized_jobs` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `afe_parameterized_jobs_profilers`
--

DROP TABLE IF EXISTS `afe_parameterized_jobs_profilers`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `afe_parameterized_jobs_profilers` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `parameterized_job_id` int(11) NOT NULL,
  `profiler_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `afe_parameterized_jobs_profilers_unique` (`parameterized_job_id`,`profiler_id`),
  KEY `afe_parameterized_jobs_profilers_profile_ibfk` (`profiler_id`),
  CONSTRAINT `afe_parameterized_jobs_profilers_profile_ibfk` FOREIGN KEY (`profiler_id`) REFERENCES `afe_profilers` (`id`),
  CONSTRAINT `afe_parameterized_jobs_profilers_parameterized_job_ibfk` FOREIGN KEY (`parameterized_job_id`) REFERENCES `afe_parameterized_jobs` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `afe_profilers`
--

DROP TABLE IF EXISTS `afe_profilers`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `afe_profilers` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `name` varchar(255) NOT NULL,
  `description` longtext NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `name` (`name`)
) ENGINE=InnoDB AUTO_INCREMENT=18 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `afe_recurring_run`
--

DROP TABLE IF EXISTS `afe_recurring_run`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `afe_recurring_run` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `job_id` int(11) NOT NULL,
  `owner_id` int(11) NOT NULL,
  `start_date` datetime NOT NULL,
  `loop_period` int(11) NOT NULL,
  `loop_count` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  KEY `recurring_run_job_id` (`job_id`),
  KEY `recurring_run_owner_id` (`owner_id`),
  CONSTRAINT `recurring_run_job_id_fk` FOREIGN KEY (`job_id`) REFERENCES `afe_jobs` (`id`) ON DELETE NO ACTION,
  CONSTRAINT `recurring_run_owner_id_fk` FOREIGN KEY (`owner_id`) REFERENCES `afe_users` (`id`) ON DELETE NO ACTION
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `afe_replaced_labels`
--

DROP TABLE IF EXISTS `afe_replaced_labels`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `afe_replaced_labels` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `label_id` int(11) DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `label_id` (`label_id`),
  CONSTRAINT `afe_replaced_labels_ibfk_1` FOREIGN KEY (`label_id`) REFERENCES `afe_labels` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `afe_shards`
--

DROP TABLE IF EXISTS `afe_shards`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `afe_shards` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `hostname` varchar(255) NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `afe_shards_labels`
--

DROP TABLE IF EXISTS `afe_shards_labels`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `afe_shards_labels` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `shard_id` int(11) NOT NULL,
  `label_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `shard_label_id_uc` (`label_id`),
  KEY `shard_shard_id_fk` (`shard_id`),
  CONSTRAINT `shard_label_id_fk` FOREIGN KEY (`label_id`) REFERENCES `afe_labels` (`id`),
  CONSTRAINT `shard_shard_id_fk` FOREIGN KEY (`shard_id`) REFERENCES `afe_shards` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `afe_special_tasks`
--

DROP TABLE IF EXISTS `afe_special_tasks`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `afe_special_tasks` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `host_id` int(11) NOT NULL,
  `task` varchar(64) NOT NULL,
  `time_requested` datetime NOT NULL,
  `is_active` tinyint(1) NOT NULL DEFAULT '0',
  `is_complete` tinyint(1) NOT NULL DEFAULT '0',
  `time_started` datetime DEFAULT NULL,
  `queue_entry_id` int(11) DEFAULT NULL,
  `success` tinyint(1) NOT NULL DEFAULT '0',
  `requested_by_id` int(11) NOT NULL,
  `is_aborted` tinyint(1) NOT NULL DEFAULT '0',
  `time_finished` datetime DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `special_tasks_host_id` (`host_id`),
  KEY `special_tasks_host_queue_entry_id` (`queue_entry_id`),
  KEY `special_tasks_requested_by_id` (`requested_by_id`),
  KEY `special_tasks_active_complete` (`is_active`,`is_complete`),
  CONSTRAINT `special_tasks_requested_by_id` FOREIGN KEY (`requested_by_id`) REFERENCES `afe_users` (`id`) ON DELETE NO ACTION,
  CONSTRAINT `special_tasks_to_hosts_ibfk` FOREIGN KEY (`host_id`) REFERENCES `afe_hosts` (`id`),
  CONSTRAINT `special_tasks_to_host_queue_entries_ibfk` FOREIGN KEY (`queue_entry_id`) REFERENCES `afe_host_queue_entries` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `afe_stable_versions`
--

DROP TABLE IF EXISTS `afe_stable_versions`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `afe_stable_versions` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `board` varchar(255) NOT NULL,
  `version` varchar(255) NOT NULL,
  `archive_url` text,
  PRIMARY KEY (`id`),
  UNIQUE KEY `board_UNIQUE` (`board`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `afe_static_host_attributes`
--

DROP TABLE IF EXISTS `afe_static_host_attributes`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `afe_static_host_attributes` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `host_id` int(11) NOT NULL,
  `attribute` varchar(90) NOT NULL,
  `value` varchar(300) NOT NULL,
  PRIMARY KEY (`id`),
  KEY `host_id` (`host_id`),
  KEY `attribute` (`attribute`),
  CONSTRAINT `afe_static_host_attributes_ibfk_1` FOREIGN KEY (`host_id`) REFERENCES `afe_hosts` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `afe_static_hosts_labels`
--

DROP TABLE IF EXISTS `afe_static_hosts_labels`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `afe_static_hosts_labels` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `host_id` int(11) DEFAULT NULL,
  `staticlabel_id` int(11) DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hosts_labels_both_ids` (`staticlabel_id`,`host_id`),
  KEY `hosts_labels_host_id` (`host_id`),
  CONSTRAINT `static_hosts_labels_host_id_fk` FOREIGN KEY (`host_id`) REFERENCES `afe_hosts` (`id`) ON DELETE NO ACTION,
  CONSTRAINT `static_hosts_labels_label_id_fk` FOREIGN KEY (`staticlabel_id`) REFERENCES `afe_static_labels` (`id`) ON DELETE NO ACTION
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `afe_static_labels`
--

DROP TABLE IF EXISTS `afe_static_labels`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `afe_static_labels` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `name` varchar(750) DEFAULT NULL,
  `kernel_config` varchar(255) DEFAULT NULL,
  `platform` tinyint(1) DEFAULT '0',
  `invalid` tinyint(1) NOT NULL,
  `only_if_needed` tinyint(1) NOT NULL,
  `atomic_group_id` int(11) DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `name` (`name`(50)),
  KEY `atomic_group_id` (`atomic_group_id`),
  CONSTRAINT `afe_static_labels_idfk_1` FOREIGN KEY (`atomic_group_id`) REFERENCES `afe_atomic_groups` (`id`) ON DELETE NO ACTION
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `afe_test_parameters`
--

DROP TABLE IF EXISTS `afe_test_parameters`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `afe_test_parameters` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `test_id` int(11) NOT NULL,
  `name` varchar(255) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `afe_test_parameters_unique` (`test_id`,`name`),
  CONSTRAINT `afe_test_parameters_test_ibfk` FOREIGN KEY (`test_id`) REFERENCES `afe_autotests` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `afe_users`
--

DROP TABLE IF EXISTS `afe_users`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `afe_users` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `login` varchar(255) DEFAULT NULL,
  `access_level` int(11) DEFAULT '0',
  `reboot_before` smallint(6) NOT NULL,
  `reboot_after` smallint(6) NOT NULL,
  `show_experimental` tinyint(1) NOT NULL DEFAULT '0',
  `drone_set_id` int(11) DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `login_unique` (`login`),
  KEY `afe_users_drone_set_ibfk` (`drone_set_id`),
  CONSTRAINT `afe_users_drone_set_ibfk` FOREIGN KEY (`drone_set_id`) REFERENCES `afe_drone_sets` (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=2 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `auth_group`
--

DROP TABLE IF EXISTS `auth_group`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `auth_group` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `name` varchar(80) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `name` (`name`)
) ENGINE=InnoDB AUTO_INCREMENT=2 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `auth_group_permissions`
--

DROP TABLE IF EXISTS `auth_group_permissions`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `auth_group_permissions` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `group_id` int(11) NOT NULL,
  `permission_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `group_id` (`group_id`,`permission_id`),
  KEY `auth_group_permissions_5f412f9a` (`group_id`),
  KEY `auth_group_permissions_83d7f98b` (`permission_id`),
  CONSTRAINT `group_id_refs_id_f4b32aac` FOREIGN KEY (`group_id`) REFERENCES `auth_group` (`id`),
  CONSTRAINT `permission_id_refs_id_6ba0f519` FOREIGN KEY (`permission_id`) REFERENCES `auth_permission` (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=25 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `auth_permission`
--

DROP TABLE IF EXISTS `auth_permission`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `auth_permission` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `name` varchar(50) NOT NULL,
  `content_type_id` int(11) NOT NULL,
  `codename` varchar(100) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `content_type_id` (`content_type_id`,`codename`),
  KEY `auth_permission_37ef4eb4` (`content_type_id`),
  CONSTRAINT `content_type_id_refs_id_d043b34a` FOREIGN KEY (`content_type_id`) REFERENCES `django_content_type` (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=136 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `auth_user`
--

DROP TABLE IF EXISTS `auth_user`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `auth_user` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `password` varchar(128) NOT NULL,
  `last_login` datetime NOT NULL,
  `is_superuser` tinyint(1) NOT NULL,
  `username` varchar(30) NOT NULL,
  `first_name` varchar(30) NOT NULL,
  `last_name` varchar(30) NOT NULL,
  `email` varchar(75) NOT NULL,
  `is_staff` tinyint(1) NOT NULL,
  `is_active` tinyint(1) NOT NULL,
  `date_joined` datetime NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `username` (`username`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `auth_user_groups`
--

DROP TABLE IF EXISTS `auth_user_groups`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `auth_user_groups` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `user_id` int(11) NOT NULL,
  `group_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `user_id` (`user_id`,`group_id`),
  KEY `auth_user_groups_6340c63c` (`user_id`),
  KEY `auth_user_groups_5f412f9a` (`group_id`),
  CONSTRAINT `user_id_refs_id_40c41112` FOREIGN KEY (`user_id`) REFERENCES `auth_user` (`id`),
  CONSTRAINT `group_id_refs_id_274b862c` FOREIGN KEY (`group_id`) REFERENCES `auth_group` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `auth_user_user_permissions`
--

DROP TABLE IF EXISTS `auth_user_user_permissions`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `auth_user_user_permissions` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `user_id` int(11) NOT NULL,
  `permission_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `user_id` (`user_id`,`permission_id`),
  KEY `auth_user_user_permissions_6340c63c` (`user_id`),
  KEY `auth_user_user_permissions_83d7f98b` (`permission_id`),
  CONSTRAINT `user_id_refs_id_4dc23c39` FOREIGN KEY (`user_id`) REFERENCES `auth_user` (`id`),
  CONSTRAINT `permission_id_refs_id_35d9ac25` FOREIGN KEY (`permission_id`) REFERENCES `auth_permission` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `django_admin_log`
--

DROP TABLE IF EXISTS `django_admin_log`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `django_admin_log` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `action_time` datetime NOT NULL,
  `user_id` int(11) NOT NULL,
  `content_type_id` int(11) DEFAULT NULL,
  `object_id` longtext,
  `object_repr` varchar(200) NOT NULL,
  `action_flag` smallint(5) unsigned NOT NULL,
  `change_message` longtext NOT NULL,
  PRIMARY KEY (`id`),
  KEY `django_admin_log_6340c63c` (`user_id`),
  KEY `django_admin_log_37ef4eb4` (`content_type_id`),
  CONSTRAINT `content_type_id_refs_id_93d2d1f8` FOREIGN KEY (`content_type_id`) REFERENCES `django_content_type` (`id`),
  CONSTRAINT `user_id_refs_id_c0d12874` FOREIGN KEY (`user_id`) REFERENCES `auth_user` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `django_content_type`
--

DROP TABLE IF EXISTS `django_content_type`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `django_content_type` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `name` varchar(100) NOT NULL,
  `app_label` varchar(100) NOT NULL,
  `model` varchar(100) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `app_label` (`app_label`,`model`)
) ENGINE=InnoDB AUTO_INCREMENT=46 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `django_session`
--

DROP TABLE IF EXISTS `django_session`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `django_session` (
  `session_key` varchar(40) NOT NULL,
  `session_data` longtext NOT NULL,
  `expire_date` datetime NOT NULL,
  PRIMARY KEY (`session_key`),
  KEY `django_session_b7b81f0c` (`expire_date`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `django_site`
--

DROP TABLE IF EXISTS `django_site`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `django_site` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `domain` varchar(100) NOT NULL,
  `name` varchar(50) NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=2 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `migrate_info`
--

DROP TABLE IF EXISTS `migrate_info`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `migrate_info` (
  `version` int(11) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `tko_embedded_graphing_queries`
--

DROP TABLE IF EXISTS `tko_embedded_graphing_queries`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tko_embedded_graphing_queries` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `url_token` text NOT NULL,
  `graph_type` varchar(16) NOT NULL,
  `params` text NOT NULL,
  `last_updated` datetime NOT NULL,
  `refresh_time` datetime DEFAULT NULL,
  `cached_png` mediumblob,
  PRIMARY KEY (`id`),
  KEY `url_token` (`url_token`(128))
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `tko_iteration_attributes`
--

DROP TABLE IF EXISTS `tko_iteration_attributes`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tko_iteration_attributes` (
  `test_idx` int(10) unsigned NOT NULL,
  `iteration` int(11) DEFAULT NULL,
  `attribute` varchar(30) DEFAULT NULL,
  `value` varchar(1024) DEFAULT NULL,
  KEY `test_idx` (`test_idx`),
  CONSTRAINT `tko_iteration_attributes_ibfk_1` FOREIGN KEY (`test_idx`) REFERENCES `tko_tests` (`test_idx`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `tko_iteration_perf_value`
--

DROP TABLE IF EXISTS `tko_iteration_perf_value`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tko_iteration_perf_value` (
  `test_idx` int(10) unsigned NOT NULL,
  `iteration` int(11) DEFAULT NULL,
  `description` varchar(256) DEFAULT NULL,
  `value` float DEFAULT NULL,
  `stddev` float DEFAULT NULL,
  `units` varchar(32) DEFAULT NULL,
  `higher_is_better` tinyint(1) NOT NULL DEFAULT '1',
  `graph` varchar(256) DEFAULT NULL,
  KEY `test_idx` (`test_idx`),
  KEY `description` (`description`(255)),
  KEY `value` (`value`),
  CONSTRAINT `tko_iteration_perf_value_ibfk` FOREIGN KEY (`test_idx`) REFERENCES `tko_tests` (`test_idx`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `tko_iteration_result`
--

DROP TABLE IF EXISTS `tko_iteration_result`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tko_iteration_result` (
  `test_idx` int(10) unsigned NOT NULL,
  `iteration` int(11) DEFAULT NULL,
  `attribute` varchar(256) DEFAULT NULL,
  `value` float DEFAULT NULL,
  KEY `test_idx` (`test_idx`),
  KEY `attribute` (`attribute`),
  KEY `value` (`value`),
  CONSTRAINT `tko_iteration_result_ibfk_1` FOREIGN KEY (`test_idx`) REFERENCES `tko_tests` (`test_idx`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `tko_job_keyvals`
--

DROP TABLE IF EXISTS `tko_job_keyvals`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tko_job_keyvals` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `job_id` int(10) unsigned NOT NULL,
  `key` varchar(90) NOT NULL,
  `value` varchar(300) NOT NULL,
  PRIMARY KEY (`id`),
  KEY `tko_job_keyvals_job_id` (`job_id`),
  KEY `tko_job_keyvals_key` (`key`),
  CONSTRAINT `tko_job_keyvals_ibfk_1` FOREIGN KEY (`job_id`) REFERENCES `tko_jobs` (`job_idx`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `tko_jobs`
--

DROP TABLE IF EXISTS `tko_jobs`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tko_jobs` (
  `job_idx` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `tag` varchar(100) DEFAULT NULL,
  `label` varchar(100) DEFAULT NULL,
  `username` varchar(80) DEFAULT NULL,
  `machine_idx` int(10) unsigned NOT NULL,
  `queued_time` datetime DEFAULT NULL,
  `started_time` datetime DEFAULT NULL,
  `finished_time` datetime DEFAULT NULL,
  `afe_job_id` int(11) DEFAULT NULL,
  `afe_parent_job_id` int(11) DEFAULT NULL,
  `build` varchar(255) DEFAULT NULL,
  `build_version` varchar(255) DEFAULT NULL,
  `suite` varchar(40) DEFAULT NULL,
  `board` varchar(40) DEFAULT NULL,
  PRIMARY KEY (`job_idx`),
  UNIQUE KEY `tag` (`tag`),
  KEY `label` (`label`),
  KEY `username` (`username`),
  KEY `machine_idx` (`machine_idx`),
  KEY `afe_job_id` (`afe_job_id`),
  KEY `afe_parent_job_id` (`afe_parent_job_id`),
  KEY `build` (`build`),
  KEY `build_version_suite_board` (`build_version`,`suite`,`board`),
  KEY `started_time_index` (`started_time`),
  KEY `queued_time` (`queued_time`),
  KEY `finished_time` (`finished_time`),
  CONSTRAINT `tko_jobs_ibfk_1` FOREIGN KEY (`machine_idx`) REFERENCES `tko_machines` (`machine_idx`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `tko_kernels`
--

DROP TABLE IF EXISTS `tko_kernels`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tko_kernels` (
  `kernel_idx` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `kernel_hash` varchar(35) DEFAULT NULL,
  `base` varchar(30) DEFAULT NULL,
  `printable` varchar(100) DEFAULT NULL,
  PRIMARY KEY (`kernel_idx`),
  KEY `printable` (`printable`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `tko_machines`
--

DROP TABLE IF EXISTS `tko_machines`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tko_machines` (
  `machine_idx` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `hostname` varchar(700) DEFAULT NULL,
  `machine_group` varchar(80) DEFAULT NULL,
  `owner` varchar(80) DEFAULT NULL,
  PRIMARY KEY (`machine_idx`),
  UNIQUE KEY `hostname` (`hostname`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `tko_patches`
--

DROP TABLE IF EXISTS `tko_patches`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tko_patches` (
  `kernel_idx` int(10) unsigned NOT NULL,
  `name` varchar(80) DEFAULT NULL,
  `url` varchar(300) DEFAULT NULL,
  `hash` varchar(35) DEFAULT NULL,
  KEY `kernel_idx` (`kernel_idx`),
  CONSTRAINT `tko_patches_ibfk_1` FOREIGN KEY (`kernel_idx`) REFERENCES `tko_kernels` (`kernel_idx`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Temporary table structure for view `tko_perf_view`
--

DROP TABLE IF EXISTS `tko_perf_view`;
/*!50001 DROP VIEW IF EXISTS `tko_perf_view`*/;
SET @saved_cs_client     = @@character_set_client;
SET character_set_client = utf8;
/*!50001 CREATE TABLE `tko_perf_view` (
  `test_idx` tinyint NOT NULL,
  `job_idx` tinyint NOT NULL,
  `test` tinyint NOT NULL,
  `subdir` tinyint NOT NULL,
  `kernel_idx` tinyint NOT NULL,
  `status` tinyint NOT NULL,
  `reason` tinyint NOT NULL,
  `machine_idx` tinyint NOT NULL,
  `test_started_time` tinyint NOT NULL,
  `test_finished_time` tinyint NOT NULL,
  `job_tag` tinyint NOT NULL,
  `job_label` tinyint NOT NULL,
  `job_username` tinyint NOT NULL,
  `job_queued_time` tinyint NOT NULL,
  `job_started_time` tinyint NOT NULL,
  `job_finished_time` tinyint NOT NULL,
  `machine_hostname` tinyint NOT NULL,
  `machine_group` tinyint NOT NULL,
  `machine_owner` tinyint NOT NULL,
  `kernel_hash` tinyint NOT NULL,
  `kernel_base` tinyint NOT NULL,
  `kernel_printable` tinyint NOT NULL,
  `status_word` tinyint NOT NULL,
  `iteration` tinyint NOT NULL,
  `iteration_key` tinyint NOT NULL,
  `iteration_value` tinyint NOT NULL
) ENGINE=MyISAM */;
SET character_set_client = @saved_cs_client;

--
-- Temporary table structure for view `tko_perf_view_2`
--

DROP TABLE IF EXISTS `tko_perf_view_2`;
/*!50001 DROP VIEW IF EXISTS `tko_perf_view_2`*/;
SET @saved_cs_client     = @@character_set_client;
SET character_set_client = utf8;
/*!50001 CREATE TABLE `tko_perf_view_2` (
  `test_idx` tinyint NOT NULL,
  `job_idx` tinyint NOT NULL,
  `test_name` tinyint NOT NULL,
  `subdir` tinyint NOT NULL,
  `kernel_idx` tinyint NOT NULL,
  `status_idx` tinyint NOT NULL,
  `reason` tinyint NOT NULL,
  `machine_idx` tinyint NOT NULL,
  `test_started_time` tinyint NOT NULL,
  `test_finished_time` tinyint NOT NULL,
  `job_tag` tinyint NOT NULL,
  `job_name` tinyint NOT NULL,
  `job_owner` tinyint NOT NULL,
  `job_queued_time` tinyint NOT NULL,
  `job_started_time` tinyint NOT NULL,
  `job_finished_time` tinyint NOT NULL,
  `hostname` tinyint NOT NULL,
  `platform` tinyint NOT NULL,
  `machine_owner` tinyint NOT NULL,
  `kernel_hash` tinyint NOT NULL,
  `kernel_base` tinyint NOT NULL,
  `kernel` tinyint NOT NULL,
  `status` tinyint NOT NULL,
  `iteration` tinyint NOT NULL,
  `iteration_key` tinyint NOT NULL,
  `iteration_value` tinyint NOT NULL
) ENGINE=MyISAM */;
SET character_set_client = @saved_cs_client;

--
-- Table structure for table `tko_query_history`
--

DROP TABLE IF EXISTS `tko_query_history`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tko_query_history` (
  `uid` varchar(32) DEFAULT NULL,
  `time_created` varchar(32) DEFAULT NULL,
  `user_comment` varchar(256) DEFAULT NULL,
  `url` varchar(1000) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `tko_saved_queries`
--

DROP TABLE IF EXISTS `tko_saved_queries`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tko_saved_queries` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `owner` varchar(80) NOT NULL,
  `name` varchar(100) NOT NULL,
  `url_token` longtext NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `tko_status`
--

DROP TABLE IF EXISTS `tko_status`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tko_status` (
  `status_idx` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `word` varchar(10) DEFAULT NULL,
  PRIMARY KEY (`status_idx`),
  KEY `word` (`word`)
) ENGINE=InnoDB AUTO_INCREMENT=10 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `tko_status`
--

LOCK TABLES `tko_status` WRITE;
/*!40000 ALTER TABLE `tko_status` DISABLE KEYS */;
INSERT INTO `tko_status` (word) VALUES ('ABORT'),('ALERT'),('ERROR'),('FAIL'),('GOOD'),('NOSTATUS'),('RUNNING'),('TEST_NA'),('WARN');
/*!40000 ALTER TABLE `tko_status` ENABLE KEYS */;
UNLOCK TABLES;                     
                     
--
-- Table structure for table `tko_task_references`
--

DROP TABLE IF EXISTS `tko_task_references`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tko_task_references` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `reference_type` enum('skylab','afe') NOT NULL,
  `tko_job_idx` int(10) unsigned NOT NULL,
  `task_id` varchar(20) DEFAULT NULL,
  `parent_task_id` varchar(20) DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `tko_task_references_ibfk_1` (`tko_job_idx`),
  KEY `reference_type_id` (`reference_type`,`id`),
  KEY `reference_type_task_id` (`reference_type`,`task_id`),
  KEY `reference_type_parent_task_id` (`reference_type`,`parent_task_id`),
  CONSTRAINT `tko_task_references_ibfk_1` FOREIGN KEY (`tko_job_idx`) REFERENCES `tko_jobs` (`job_idx`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `tko_test_attributes`
--

DROP TABLE IF EXISTS `tko_test_attributes`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tko_test_attributes` (
  `test_idx` int(10) unsigned NOT NULL,
  `attribute` varchar(30) DEFAULT NULL,
  `value` varchar(1024) DEFAULT NULL,
  `id` bigint(20) NOT NULL AUTO_INCREMENT,
  `user_created` tinyint(1) NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  KEY `test_idx` (`test_idx`),
  KEY `attribute` (`attribute`),
  KEY `value` (`value`(767)),
  CONSTRAINT `tko_test_attributes_ibfk_1` FOREIGN KEY (`test_idx`) REFERENCES `tko_tests` (`test_idx`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `tko_test_labels`
--

DROP TABLE IF EXISTS `tko_test_labels`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tko_test_labels` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `name` varchar(80) NOT NULL,
  `description` longtext NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `tko_test_labels_unique` (`name`)
) ENGINE=InnoDB AUTO_INCREMENT=2 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `tko_test_labels_tests`
--

DROP TABLE IF EXISTS `tko_test_labels_tests`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tko_test_labels_tests` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `testlabel_id` int(11) NOT NULL,
  `test_id` int(10) unsigned NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `testlabel_id` (`testlabel_id`,`test_id`),
  KEY `test_labels_tests_test_id` (`test_id`),
  CONSTRAINT `tests_labels_tests_ibfk_2` FOREIGN KEY (`test_id`) REFERENCES `tko_tests` (`test_idx`) ON DELETE CASCADE,
  CONSTRAINT `tko_test_labels_tests_ibfk_1` FOREIGN KEY (`testlabel_id`) REFERENCES `tko_test_labels` (`id`) ON DELETE CASCADE,
  CONSTRAINT `tko_test_labels_tests_ibfk_2` FOREIGN KEY (`test_id`) REFERENCES `tko_tests` (`test_idx`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Temporary table structure for view `tko_test_view`
--

DROP TABLE IF EXISTS `tko_test_view`;
/*!50001 DROP VIEW IF EXISTS `tko_test_view`*/;
SET @saved_cs_client     = @@character_set_client;
SET character_set_client = utf8;
/*!50001 CREATE TABLE `tko_test_view` (
  `test_idx` tinyint NOT NULL,
  `job_idx` tinyint NOT NULL,
  `test` tinyint NOT NULL,
  `subdir` tinyint NOT NULL,
  `kernel_idx` tinyint NOT NULL,
  `status` tinyint NOT NULL,
  `reason` tinyint NOT NULL,
  `machine_idx` tinyint NOT NULL,
  `test_started_time` tinyint NOT NULL,
  `test_finished_time` tinyint NOT NULL,
  `job_tag` tinyint NOT NULL,
  `job_label` tinyint NOT NULL,
  `job_username` tinyint NOT NULL,
  `job_queued_time` tinyint NOT NULL,
  `job_started_time` tinyint NOT NULL,
  `job_finished_time` tinyint NOT NULL,
  `machine_hostname` tinyint NOT NULL,
  `machine_group` tinyint NOT NULL,
  `machine_owner` tinyint NOT NULL,
  `kernel_hash` tinyint NOT NULL,
  `kernel_base` tinyint NOT NULL,
  `kernel_printable` tinyint NOT NULL,
  `status_word` tinyint NOT NULL
) ENGINE=MyISAM */;
SET character_set_client = @saved_cs_client;

--
-- Temporary table structure for view `tko_test_view_2`
--

DROP TABLE IF EXISTS `tko_test_view_2`;
/*!50001 DROP VIEW IF EXISTS `tko_test_view_2`*/;
SET @saved_cs_client     = @@character_set_client;
SET character_set_client = utf8;
/*!50001 CREATE TABLE `tko_test_view_2` (
  `test_idx` tinyint NOT NULL,
  `job_idx` tinyint NOT NULL,
  `test_name` tinyint NOT NULL,
  `subdir` tinyint NOT NULL,
  `kernel_idx` tinyint NOT NULL,
  `status_idx` tinyint NOT NULL,
  `reason` tinyint NOT NULL,
  `machine_idx` tinyint NOT NULL,
  `invalid` tinyint NOT NULL,
  `invalidates_test_idx` tinyint NOT NULL,
  `test_started_time` tinyint NOT NULL,
  `test_finished_time` tinyint NOT NULL,
  `job_tag` tinyint NOT NULL,
  `job_name` tinyint NOT NULL,
  `job_owner` tinyint NOT NULL,
  `job_queued_time` tinyint NOT NULL,
  `job_started_time` tinyint NOT NULL,
  `job_finished_time` tinyint NOT NULL,
  `afe_job_id` tinyint NOT NULL,
  `afe_parent_job_id` tinyint NOT NULL,
  `build` tinyint NOT NULL,
  `build_version` tinyint NOT NULL,
  `suite` tinyint NOT NULL,
  `board` tinyint NOT NULL,
  `hostname` tinyint NOT NULL,
  `platform` tinyint NOT NULL,
  `machine_owner` tinyint NOT NULL,
  `kernel_hash` tinyint NOT NULL,
  `kernel_base` tinyint NOT NULL,
  `kernel` tinyint NOT NULL,
  `status` tinyint NOT NULL
) ENGINE=MyISAM */;
SET character_set_client = @saved_cs_client;

--
-- Temporary table structure for view `tko_test_view_outer_joins`
--

DROP TABLE IF EXISTS `tko_test_view_outer_joins`;
/*!50001 DROP VIEW IF EXISTS `tko_test_view_outer_joins`*/;
SET @saved_cs_client     = @@character_set_client;
SET character_set_client = utf8;
/*!50001 CREATE TABLE `tko_test_view_outer_joins` (
  `test_idx` tinyint NOT NULL,
  `job_idx` tinyint NOT NULL,
  `test_name` tinyint NOT NULL,
  `subdir` tinyint NOT NULL,
  `kernel_idx` tinyint NOT NULL,
  `status_idx` tinyint NOT NULL,
  `reason` tinyint NOT NULL,
  `machine_idx` tinyint NOT NULL,
  `test_started_time` tinyint NOT NULL,
  `test_finished_time` tinyint NOT NULL,
  `job_tag` tinyint NOT NULL,
  `job_name` tinyint NOT NULL,
  `job_owner` tinyint NOT NULL,
  `job_queued_time` tinyint NOT NULL,
  `job_started_time` tinyint NOT NULL,
  `job_finished_time` tinyint NOT NULL,
  `hostname` tinyint NOT NULL,
  `platform` tinyint NOT NULL,
  `machine_owner` tinyint NOT NULL,
  `kernel_hash` tinyint NOT NULL,
  `kernel_base` tinyint NOT NULL,
  `kernel` tinyint NOT NULL,
  `status` tinyint NOT NULL
) ENGINE=MyISAM */;
SET character_set_client = @saved_cs_client;

--
-- Table structure for table `tko_tests`
--

DROP TABLE IF EXISTS `tko_tests`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tko_tests` (
  `test_idx` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `job_idx` int(10) unsigned NOT NULL,
  `test` varchar(300) DEFAULT NULL,
  `subdir` varchar(300) DEFAULT NULL,
  `kernel_idx` int(10) unsigned NOT NULL,
  `status` int(10) unsigned NOT NULL,
  `reason` varchar(4096) DEFAULT NULL,
  `machine_idx` int(10) unsigned NOT NULL,
  `invalid` tinyint(1) DEFAULT '0',
  `finished_time` datetime DEFAULT NULL,
  `started_time` datetime DEFAULT NULL,
  `invalidates_test_idx` int(10) unsigned DEFAULT NULL,
  PRIMARY KEY (`test_idx`),
  KEY `kernel_idx` (`kernel_idx`),
  KEY `status` (`status`),
  KEY `machine_idx` (`machine_idx`),
  KEY `job_idx` (`job_idx`),
  KEY `reason` (`reason`(767)),
  KEY `test` (`test`),
  KEY `subdir` (`subdir`),
  KEY `started_time` (`started_time`),
  KEY `invalidates_test_idx` (`invalidates_test_idx`),
  KEY `finished_time_idx` (`finished_time`),
  CONSTRAINT `invalidates_test_idx_fk_1` FOREIGN KEY (`invalidates_test_idx`) REFERENCES `tko_tests` (`test_idx`) ON DELETE CASCADE,
  CONSTRAINT `tests_to_jobs_ibfk` FOREIGN KEY (`job_idx`) REFERENCES `tko_jobs` (`job_idx`),
  CONSTRAINT `tko_tests_ibfk_1` FOREIGN KEY (`kernel_idx`) REFERENCES `tko_kernels` (`kernel_idx`) ON DELETE CASCADE,
  CONSTRAINT `tko_tests_ibfk_2` FOREIGN KEY (`status`) REFERENCES `tko_status` (`status_idx`) ON DELETE CASCADE,
  CONSTRAINT `tko_tests_ibfk_3` FOREIGN KEY (`machine_idx`) REFERENCES `tko_machines` (`machine_idx`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Final view structure for view `tko_perf_view`
--

/*!50001 DROP TABLE IF EXISTS `tko_perf_view`*/;
/*!50001 DROP VIEW IF EXISTS `tko_perf_view`*/;
/*!50001 SET @saved_cs_client          = @@character_set_client */;
/*!50001 SET @saved_cs_results         = @@character_set_results */;
/*!50001 SET @saved_col_connection     = @@collation_connection */;
/*!50001 SET character_set_client      = utf8 */;
/*!50001 SET character_set_results     = utf8 */;
/*!50001 SET collation_connection      = utf8_general_ci */;
/*!50001 CREATE ALGORITHM=UNDEFINED */
/*!50013 DEFINER=`chromeosqa-admin`@`localhost` SQL SECURITY DEFINER */
/*!50001 VIEW `tko_perf_view` AS select `tko_tests`.`test_idx` AS `test_idx`,`tko_tests`.`job_idx` AS `job_idx`,`tko_tests`.`test` AS `test`,`tko_tests`.`subdir` AS `subdir`,`tko_tests`.`kernel_idx` AS `kernel_idx`,`tko_tests`.`status` AS `status`,`tko_tests`.`reason` AS `reason`,`tko_tests`.`machine_idx` AS `machine_idx`,`tko_tests`.`started_time` AS `test_started_time`,`tko_tests`.`finished_time` AS `test_finished_time`,`tko_jobs`.`tag` AS `job_tag`,`tko_jobs`.`label` AS `job_label`,`tko_jobs`.`username` AS `job_username`,`tko_jobs`.`queued_time` AS `job_queued_time`,`tko_jobs`.`started_time` AS `job_started_time`,`tko_jobs`.`finished_time` AS `job_finished_time`,`tko_machines`.`hostname` AS `machine_hostname`,`tko_machines`.`machine_group` AS `machine_group`,`tko_machines`.`owner` AS `machine_owner`,`tko_kernels`.`kernel_hash` AS `kernel_hash`,`tko_kernels`.`base` AS `kernel_base`,`tko_kernels`.`printable` AS `kernel_printable`,`tko_status`.`word` AS `status_word`,`tko_iteration_result`.`iteration` AS `iteration`,`tko_iteration_result`.`attribute` AS `iteration_key`,`tko_iteration_result`.`value` AS `iteration_value` from (((((`tko_tests` join `tko_jobs` on((`tko_jobs`.`job_idx` = `tko_tests`.`job_idx`))) join `tko_machines` on((`tko_machines`.`machine_idx` = `tko_jobs`.`machine_idx`))) join `tko_kernels` on((`tko_kernels`.`kernel_idx` = `tko_tests`.`kernel_idx`))) join `tko_status` on((`tko_status`.`status_idx` = `tko_tests`.`status`))) join `tko_iteration_result` on((`tko_iteration_result`.`test_idx` = `tko_tests`.`test_idx`))) */;
/*!50001 SET character_set_client      = @saved_cs_client */;
/*!50001 SET character_set_results     = @saved_cs_results */;
/*!50001 SET collation_connection      = @saved_col_connection */;

--
-- Final view structure for view `tko_perf_view_2`
--

/*!50001 DROP TABLE IF EXISTS `tko_perf_view_2`*/;
/*!50001 DROP VIEW IF EXISTS `tko_perf_view_2`*/;
/*!50001 SET @saved_cs_client          = @@character_set_client */;
/*!50001 SET @saved_cs_results         = @@character_set_results */;
/*!50001 SET @saved_col_connection     = @@collation_connection */;
/*!50001 SET character_set_client      = utf8 */;
/*!50001 SET character_set_results     = utf8 */;
/*!50001 SET collation_connection      = utf8_general_ci */;
/*!50001 CREATE ALGORITHM=UNDEFINED */
/*!50013 DEFINER=`chromeosqa-admin`@`localhost` SQL SECURITY DEFINER */
/*!50001 VIEW `tko_perf_view_2` AS select `tko_tests`.`test_idx` AS `test_idx`,`tko_tests`.`job_idx` AS `job_idx`,`tko_tests`.`test` AS `test_name`,`tko_tests`.`subdir` AS `subdir`,`tko_tests`.`kernel_idx` AS `kernel_idx`,`tko_tests`.`status` AS `status_idx`,`tko_tests`.`reason` AS `reason`,`tko_tests`.`machine_idx` AS `machine_idx`,`tko_tests`.`started_time` AS `test_started_time`,`tko_tests`.`finished_time` AS `test_finished_time`,`tko_jobs`.`tag` AS `job_tag`,`tko_jobs`.`label` AS `job_name`,`tko_jobs`.`username` AS `job_owner`,`tko_jobs`.`queued_time` AS `job_queued_time`,`tko_jobs`.`started_time` AS `job_started_time`,`tko_jobs`.`finished_time` AS `job_finished_time`,`tko_machines`.`hostname` AS `hostname`,`tko_machines`.`machine_group` AS `platform`,`tko_machines`.`owner` AS `machine_owner`,`tko_kernels`.`kernel_hash` AS `kernel_hash`,`tko_kernels`.`base` AS `kernel_base`,`tko_kernels`.`printable` AS `kernel`,`tko_status`.`word` AS `status`,`tko_iteration_result`.`iteration` AS `iteration`,`tko_iteration_result`.`attribute` AS `iteration_key`,`tko_iteration_result`.`value` AS `iteration_value` from (((((`tko_tests` left join `tko_jobs` on((`tko_jobs`.`job_idx` = `tko_tests`.`job_idx`))) left join `tko_machines` on((`tko_machines`.`machine_idx` = `tko_jobs`.`machine_idx`))) left join `tko_kernels` on((`tko_kernels`.`kernel_idx` = `tko_tests`.`kernel_idx`))) left join `tko_status` on((`tko_status`.`status_idx` = `tko_tests`.`status`))) left join `tko_iteration_result` on((`tko_iteration_result`.`test_idx` = `tko_tests`.`test_idx`))) */;
/*!50001 SET character_set_client      = @saved_cs_client */;
/*!50001 SET character_set_results     = @saved_cs_results */;
/*!50001 SET collation_connection      = @saved_col_connection */;

--
-- Final view structure for view `tko_test_view`
--

/*!50001 DROP TABLE IF EXISTS `tko_test_view`*/;
/*!50001 DROP VIEW IF EXISTS `tko_test_view`*/;
/*!50001 SET @saved_cs_client          = @@character_set_client */;
/*!50001 SET @saved_cs_results         = @@character_set_results */;
/*!50001 SET @saved_col_connection     = @@collation_connection */;
/*!50001 SET character_set_client      = utf8 */;
/*!50001 SET character_set_results     = utf8 */;
/*!50001 SET collation_connection      = utf8_general_ci */;
/*!50001 CREATE ALGORITHM=UNDEFINED */
/*!50013 DEFINER=`chromeosqa-admin`@`localhost` SQL SECURITY DEFINER */
/*!50001 VIEW `tko_test_view` AS select `tko_tests`.`test_idx` AS `test_idx`,`tko_tests`.`job_idx` AS `job_idx`,`tko_tests`.`test` AS `test`,`tko_tests`.`subdir` AS `subdir`,`tko_tests`.`kernel_idx` AS `kernel_idx`,`tko_tests`.`status` AS `status`,`tko_tests`.`reason` AS `reason`,`tko_tests`.`machine_idx` AS `machine_idx`,`tko_tests`.`started_time` AS `test_started_time`,`tko_tests`.`finished_time` AS `test_finished_time`,`tko_jobs`.`tag` AS `job_tag`,`tko_jobs`.`label` AS `job_label`,`tko_jobs`.`username` AS `job_username`,`tko_jobs`.`queued_time` AS `job_queued_time`,`tko_jobs`.`started_time` AS `job_started_time`,`tko_jobs`.`finished_time` AS `job_finished_time`,`tko_machines`.`hostname` AS `machine_hostname`,`tko_machines`.`machine_group` AS `machine_group`,`tko_machines`.`owner` AS `machine_owner`,`tko_kernels`.`kernel_hash` AS `kernel_hash`,`tko_kernels`.`base` AS `kernel_base`,`tko_kernels`.`printable` AS `kernel_printable`,`tko_status`.`word` AS `status_word` from ((((`tko_tests` join `tko_jobs` on((`tko_jobs`.`job_idx` = `tko_tests`.`job_idx`))) join `tko_machines` on((`tko_machines`.`machine_idx` = `tko_jobs`.`machine_idx`))) join `tko_kernels` on((`tko_kernels`.`kernel_idx` = `tko_tests`.`kernel_idx`))) join `tko_status` on((`tko_status`.`status_idx` = `tko_tests`.`status`))) */;
/*!50001 SET character_set_client      = @saved_cs_client */;
/*!50001 SET character_set_results     = @saved_cs_results */;
/*!50001 SET collation_connection      = @saved_col_connection */;

--
-- Final view structure for view `tko_test_view_2`
--

/*!50001 DROP TABLE IF EXISTS `tko_test_view_2`*/;
/*!50001 DROP VIEW IF EXISTS `tko_test_view_2`*/;
/*!50001 SET @saved_cs_client          = @@character_set_client */;
/*!50001 SET @saved_cs_results         = @@character_set_results */;
/*!50001 SET @saved_col_connection     = @@collation_connection */;
/*!50001 SET character_set_client      = utf8 */;
/*!50001 SET character_set_results     = utf8 */;
/*!50001 SET collation_connection      = utf8_general_ci */;
/*!50001 CREATE ALGORITHM=UNDEFINED */
/*!50013 DEFINER=`chromeosqa-admin`@`localhost` SQL SECURITY DEFINER */
/*!50001 VIEW `tko_test_view_2` AS select `tko_tests`.`test_idx` AS `test_idx`,`tko_tests`.`job_idx` AS `job_idx`,`tko_tests`.`test` AS `test_name`,`tko_tests`.`subdir` AS `subdir`,`tko_tests`.`kernel_idx` AS `kernel_idx`,`tko_tests`.`status` AS `status_idx`,`tko_tests`.`reason` AS `reason`,`tko_tests`.`machine_idx` AS `machine_idx`,`tko_tests`.`invalid` AS `invalid`,`tko_tests`.`invalidates_test_idx` AS `invalidates_test_idx`,`tko_tests`.`started_time` AS `test_started_time`,`tko_tests`.`finished_time` AS `test_finished_time`,`tko_jobs`.`tag` AS `job_tag`,`tko_jobs`.`label` AS `job_name`,`tko_jobs`.`username` AS `job_owner`,`tko_jobs`.`queued_time` AS `job_queued_time`,`tko_jobs`.`started_time` AS `job_started_time`,`tko_jobs`.`finished_time` AS `job_finished_time`,`tko_jobs`.`afe_job_id` AS `afe_job_id`,`tko_jobs`.`afe_parent_job_id` AS `afe_parent_job_id`,`tko_jobs`.`build` AS `build`,`tko_jobs`.`build_version` AS `build_version`,`tko_jobs`.`suite` AS `suite`,`tko_jobs`.`board` AS `board`,`tko_machines`.`hostname` AS `hostname`,`tko_machines`.`machine_group` AS `platform`,`tko_machines`.`owner` AS `machine_owner`,`tko_kernels`.`kernel_hash` AS `kernel_hash`,`tko_kernels`.`base` AS `kernel_base`,`tko_kernels`.`printable` AS `kernel`,`tko_status`.`word` AS `status` from ((((`tko_tests` join `tko_jobs` on((`tko_jobs`.`job_idx` = `tko_tests`.`job_idx`))) join `tko_machines` on((`tko_machines`.`machine_idx` = `tko_jobs`.`machine_idx`))) join `tko_kernels` on((`tko_kernels`.`kernel_idx` = `tko_tests`.`kernel_idx`))) join `tko_status` on((`tko_status`.`status_idx` = `tko_tests`.`status`))) */;
/*!50001 SET character_set_client      = @saved_cs_client */;
/*!50001 SET character_set_results     = @saved_cs_results */;
/*!50001 SET collation_connection      = @saved_col_connection */;

--
-- Final view structure for view `tko_test_view_outer_joins`
--

/*!50001 DROP TABLE IF EXISTS `tko_test_view_outer_joins`*/;
/*!50001 DROP VIEW IF EXISTS `tko_test_view_outer_joins`*/;
/*!50001 SET @saved_cs_client          = @@character_set_client */;
/*!50001 SET @saved_cs_results         = @@character_set_results */;
/*!50001 SET @saved_col_connection     = @@collation_connection */;
/*!50001 SET character_set_client      = utf8 */;
/*!50001 SET character_set_results     = utf8 */;
/*!50001 SET collation_connection      = utf8_general_ci */;
/*!50001 CREATE ALGORITHM=UNDEFINED */
/*!50013 DEFINER=`chromeosqa-admin`@`localhost` SQL SECURITY DEFINER */
/*!50001 VIEW `tko_test_view_outer_joins` AS select `tko_tests`.`test_idx` AS `test_idx`,`tko_tests`.`job_idx` AS `job_idx`,`tko_tests`.`test` AS `test_name`,`tko_tests`.`subdir` AS `subdir`,`tko_tests`.`kernel_idx` AS `kernel_idx`,`tko_tests`.`status` AS `status_idx`,`tko_tests`.`reason` AS `reason`,`tko_tests`.`machine_idx` AS `machine_idx`,`tko_tests`.`started_time` AS `test_started_time`,`tko_tests`.`finished_time` AS `test_finished_time`,`tko_jobs`.`tag` AS `job_tag`,`tko_jobs`.`label` AS `job_name`,`tko_jobs`.`username` AS `job_owner`,`tko_jobs`.`queued_time` AS `job_queued_time`,`tko_jobs`.`started_time` AS `job_started_time`,`tko_jobs`.`finished_time` AS `job_finished_time`,`tko_machines`.`hostname` AS `hostname`,`tko_machines`.`machine_group` AS `platform`,`tko_machines`.`owner` AS `machine_owner`,`tko_kernels`.`kernel_hash` AS `kernel_hash`,`tko_kernels`.`base` AS `kernel_base`,`tko_kernels`.`printable` AS `kernel`,`tko_status`.`word` AS `status` from ((((`tko_tests` left join `tko_jobs` on((`tko_jobs`.`job_idx` = `tko_tests`.`job_idx`))) left join `tko_machines` on((`tko_machines`.`machine_idx` = `tko_jobs`.`machine_idx`))) left join `tko_kernels` on((`tko_kernels`.`kernel_idx` = `tko_tests`.`kernel_idx`))) left join `tko_status` on((`tko_status`.`status_idx` = `tko_tests`.`status`))) */;
/*!50001 SET character_set_client      = @saved_cs_client */;
/*!50001 SET character_set_results     = @saved_cs_results */;
/*!50001 SET collation_connection      = @saved_col_connection */;
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

-- Dump completed on 2018-06-18 15:46:13
