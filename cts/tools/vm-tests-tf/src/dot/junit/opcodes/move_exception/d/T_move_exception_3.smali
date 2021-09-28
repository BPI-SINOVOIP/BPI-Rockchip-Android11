.source "T_move_exception_3.java"
.class  public Ldot/junit/opcodes/move_exception/d/T_move_exception_3;
.super  Ljava/lang/Object;


.method public constructor <init>()V
.registers 1

       invoke-direct {v0}, Ljava/lang/Object;-><init>()V
       return-void
.end method

.method public run()V
.registers 6

:Label1
       const v1, 1
       const v2, 0
       div-int v0, v1, v2 
       
:Label2
       goto :Label4

:Label3
       move-exception v6

:Label4
       return-void

.catchall {:Label1 .. :Label2} :Label3
.end method
