# task_id can be NULL because tko_jobs.afe_job_id, which it replaces, can be
# NULL. Same for parent_task_id.
UP_SQL = """
CREATE INDEX reference_type_task_id ON tko_task_references (reference_type, task_id);
CREATE INDEX reference_type_parent_task_id ON tko_task_references (reference_type, parent_task_id);
"""

DOWN_SQL = """
DROP INDEX reference_type_task_id ON tko_task_references;
DROP INDEX reference_parent_type_task_id ON tko_task_references;
"""
