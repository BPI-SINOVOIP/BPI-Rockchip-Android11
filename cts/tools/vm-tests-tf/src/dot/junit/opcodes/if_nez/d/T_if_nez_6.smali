.source "T_ifne_6.java"
.class  public Ldot/junit/opcodes/if_nez/d/T_if_nez_6;
.super  Ljava/lang/Object;


.method public constructor <init>()V
.registers 1

       invoke-direct {v0}, Ljava/lang/Object;-><init>()V
       return-void
.end method

.method public run(D)I
.registers 6

       if-nez v4, :Label9
       const/16 v4, 1234
       return v4

:Label9
       const/4 v4, 1
       return v4
.end method
