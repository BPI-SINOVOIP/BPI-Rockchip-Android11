.source "T_if_le_6.java"
.class  public Ldot/junit/opcodes/if_le/d/T_if_le_6;
.super  Ljava/lang/Object;


.method public constructor <init>()V
.registers 1

       invoke-direct {v0}, Ljava/lang/Object;-><init>()V
       return-void
.end method

.method public run(JI)I
.registers 8

       if-le v5, v7, :Label11
       const/16 v5, 1234
       return v5

:Label11
       const/4 v5, 1
       return v5
.end method
