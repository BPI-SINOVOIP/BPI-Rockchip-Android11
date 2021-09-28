.source "T_if_gtz_9.java"
.class  public Ldot/junit/opcodes/if_gtz/d/T_if_gtz_9;
.super  Ljava/lang/Object;


.method public constructor <init>()V
.registers 1

       invoke-direct {v0}, Ljava/lang/Object;-><init>()V
       return-void
.end method

.method public run(I)Z
.registers 6

       if-gtz v5, :Label8
       const/4 v0, 0
       return v0

:Label8
       const v0, 0
       return v0
.end method
