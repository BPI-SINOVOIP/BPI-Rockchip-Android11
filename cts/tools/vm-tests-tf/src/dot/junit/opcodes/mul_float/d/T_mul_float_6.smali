.source "T_mul_float_6.java"
.class  public Ldot/junit/opcodes/mul_float/d/T_mul_float_6;
.super  Ljava/lang/Object;


.method public constructor <init>()V
.registers 1

       invoke-direct {v0}, Ljava/lang/Object;-><init>()V
       return-void
.end method

.method public run(FI)F
.registers 8

       mul-float v0, v6, v7
       return v0
.end method
