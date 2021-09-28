.source "T_neg_double_6.java"
.class  public Ldot/junit/opcodes/neg_double/d/T_neg_double_6;
.super  Ljava/lang/Object;


.method public constructor <init>()V
.registers 1

       invoke-direct {v0}, Ljava/lang/Object;-><init>()V
       return-void
.end method

.method public run(D)D
.registers 8

       neg-double v0, v5
       return-wide v0
.end method
