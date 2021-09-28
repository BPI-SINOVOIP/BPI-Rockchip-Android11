.source "T_ifne_6.java"
.class  public Ldot/junit/opcodes/if_eqz/d/T_if_eqz_6;
.super  Ljava/lang/Object;


.method public constructor <init>()V
.registers 1

       invoke-direct {v0}, Ljava/lang/Object;-><init>()V
       return-void
.end method

.method public run(D)I
.registers 8

       if-eqz v6, :Label9
       const/16 v6, 1234
       return v6

:Label9
       const/4 v6, 1
       return v6
.end method
