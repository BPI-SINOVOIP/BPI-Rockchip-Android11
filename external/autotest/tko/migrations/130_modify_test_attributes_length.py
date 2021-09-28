def migrate_up(mgr):
    mgr.execute("alter table tko_test_attributes modify column attribute varchar(50);")

def migrate_down(mgr):
    mgr.execute("alter table tko_test_attributes modify column attribute varchar(30);")
