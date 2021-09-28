.source "T_if_eq_11.java"
.class  public Ldot/junit/opcodes/if_eq/d/T_if_eq_11;
.super  Ljava/lang/Object;


.method public constructor <init>()V
.registers 1

       invoke-direct {v0}, Ljava/lang/Object;-><init>()V
       return-void
.end method

.method public run(Ljava/lang/String;Ljava/lang/String;)V
.registers 8

       if-eq v6, v7, :Label10
       const/4 v6, 0
       return-void

:Label10
       const v6, 1
       nop
       nop
       const/4 v6, 1
       return-void
.end method
