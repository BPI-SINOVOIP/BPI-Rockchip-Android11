.source "T_shl_long_4.java"
.class  public Ldot/junit/opcodes/shl_long/d/T_shl_long_4;
.super  Ljava/lang/Object;


.method public constructor <init>()V
.registers 1

       invoke-direct {v0}, Ljava/lang/Object;-><init>()V
       return-void
.end method

.method public run(II)J
.registers 11

       shl-long v0, v9, v10
       return-wide v0
.end method
