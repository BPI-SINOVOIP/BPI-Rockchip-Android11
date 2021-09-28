.source "T_neg_float_5.java"
.class  public Ldot/junit/opcodes/neg_float/d/T_neg_float_5;
.super  Ljava/lang/Object;


.method public constructor <init>()V
.registers 1

       invoke-direct {v0}, Ljava/lang/Object;-><init>()V
       return-void
.end method

.method public run(F)F
.registers 5

       neg-float v0, v3
       return v0
.end method
