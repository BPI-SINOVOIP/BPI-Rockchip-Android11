.source "T_if_eqz_10.java"
.class  public Ldot/junit/opcodes/if_eqz/d/T_if_eqz_10;
.super  Ljava/lang/Object;


.method public constructor <init>()V
.registers 1

       invoke-direct {v0}, Ljava/lang/Object;-><init>()V
       return-void
.end method

.method public run(I)Z
.registers 6

       if-eqz v5, :Label8
       const/4 v5, 0
       return v5

:Label8
       const v5, 0
       nop
       return v5
.end method
