.source "T_shl_long_2addr_5.java"
.class  public Ldot/junit/opcodes/shl_long_2addr/d/T_shl_long_2addr_5;
.super  Ljava/lang/Object;


.method public constructor <init>()V
.registers 1

       invoke-direct {v0}, Ljava/lang/Object;-><init>()V
       return-void
.end method

.method public run(FI)J
.registers 11

       shl-long/2addr v9, v10
       return-wide v9
.end method
