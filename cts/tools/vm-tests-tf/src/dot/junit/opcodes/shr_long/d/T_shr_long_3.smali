.source "T_shr_long_3.java"
.class  public Ldot/junit/opcodes/shr_long/d/T_shr_long_3;
.super  Ljava/lang/Object;


.method public constructor <init>()V
.registers 1

       invoke-direct {v0}, Ljava/lang/Object;-><init>()V
       return-void
.end method

.method public run(JD)J
.registers 11

       shr-long v0, v7, v9
       return-wide v0
.end method
