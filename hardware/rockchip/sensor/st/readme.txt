
/* -------------------------- */
gsnesor HAL 在 android 2.3 的初次集成.
从 device/htc/passion-common/libsensors 修改得到, 某些不合理的源文件名称尚未修改, 无关的其他 sensor 代码是被屏蔽状态. 

具体产品若要调整上报数据, 可以在 hardware/rk29/sensor/AkmSensor.cpp, processEvent() 中修改.

	-- 2011年 01月 04日 星期二 12:01:24 CST

/* -------------------------- */
规整起见, rk29 MID 的 gsensor HAL 上报的数据 "必须" 都以 "默认横屏" 来定义参考坐标系. 
可能需要的其他转换操作, 留在框架中完成. 

	-- 2011年 01月 06日 星期四 17:35:53 CST
