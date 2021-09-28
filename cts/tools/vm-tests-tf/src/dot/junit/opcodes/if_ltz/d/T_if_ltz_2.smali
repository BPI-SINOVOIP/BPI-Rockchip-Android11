.source "T_if_ltz_2.java"
.class  public Ldot/junit/opcodes/if_ltz/d/T_if_ltz_2;
.super  Ljava/lang/Object;


.method public constructor <init>()V
.registers 1

       invoke-direct {v0}, Ljava/lang/Object;-><init>()V
       return-void
.end method

.method public run(F)I
.registers 6

       if-ltz v5, :Label9
       const/16 v5, 1234
       return v5

:Label9
       const/4 v5, 1
       return v5
.end method
