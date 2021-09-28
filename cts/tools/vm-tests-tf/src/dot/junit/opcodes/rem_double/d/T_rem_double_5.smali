.source "T_rem_double_5.java"
.class  public Ldot/junit/opcodes/rem_double/d/T_rem_double_5;
.super  Ljava/lang/Object;


.method public constructor <init>()V
.registers 1

       invoke-direct {v0}, Ljava/lang/Object;-><init>()V
       return-void
.end method

.method public run(DD)D
.registers 14

       rem-double v0, v9, v12
       return-wide v0
.end method
