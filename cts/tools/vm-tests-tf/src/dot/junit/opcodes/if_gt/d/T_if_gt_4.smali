.source "T_if_gt_4.java"
.class  public Ldot/junit/opcodes/if_gt/d/T_if_gt_4;
.super  Ljava/lang/Object;


.method public constructor <init>()V
.registers 1

       invoke-direct {v0}, Ljava/lang/Object;-><init>()V
       return-void
.end method

.method public run(II)I
.registers 8

       if-gt v7, v8, :Label11
       const/16 v7, 1234
       return v7

:Label11
       const/4 v7, 1
       return v7
.end method
