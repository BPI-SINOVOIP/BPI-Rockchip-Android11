# task_id can be NULL because tko_jobs.afe_job_id, which it replaces, can be
# NULL. Same for parent_task_id.
UP_SQL = """
CREATE TABLE tko_task_references (
    id integer AUTO_INCREMENT NOT NULL PRIMARY KEY,
    reference_type enum('skylab', 'afe') NOT NULL,
    tko_job_idx int(10) unsigned NOT NULL,
    task_id varchar(20) DEFAULT NULL,
    parent_task_id varchar(20) DEFAULT NULL,
    CONSTRAINT tko_task_references_ibfk_1 FOREIGN KEY (tko_job_idx) REFERENCES tko_jobs (job_idx) ON DELETE CASCADE,
    KEY reference_type_id (reference_type, id)
) ENGINE=InnoDB;
"""

DOWN_SQL = """
DROP TABLE IF EXISTS tko_task_references;
"""
