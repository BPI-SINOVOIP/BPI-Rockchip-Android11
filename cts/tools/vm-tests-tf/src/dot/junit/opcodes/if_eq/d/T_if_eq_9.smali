.source "T_if_eq_9.java"
.class  public Ldot/junit/opcodes/if_eq/d/T_if_eq_9;
.super  Ljava/lang/Object;


.method public constructor <init>()V
.registers 1

       invoke-direct {v0}, Ljava/lang/Object;-><init>()V
       return-void
.end method

.method public run(ILjava/lang/String;)I
.registers 8

       if-eq v6, v7, :Label11
       const/16 v6, 1234
:Label10
       return v6
       
:Label11
       const/4 v6, 1
       goto :Label10
.end method
