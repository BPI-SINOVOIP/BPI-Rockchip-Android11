������������rk3399 7.1 sdk��ͨ��gpu������������α任���ﵽͶӰ�豸����У����Ч�����������Ѷ�У�����ڲ��ľ�ݽ����Ż���

1������˵��
�ֱ�������²���
patchs/
������ device
��   ������ rockchip
��       ������ rk3399
��       ��   ������ 0001-Keystone-close-HWC-prop-for-keystone.patch
��       ������ common
��           ������ 0001-add-properties-for-keystone-correction.patch
������ frameworks
    ������ native
        ������ 0001-PATCH-Support-keystone-correction-function-for-10.0-.patch

/src/ Ŀ¼�ṩ��Դ�ļ�����ȶ�

2��ʹ��˵��

1��ͨ���������µ�������������Ҫ�任����ı��ε��ĸ�����
	����
	persist.sys.keystone.lt=50,0		#�����ı��ξ�����Ļ���Ͻǵ�x,y����
	persist.sys.keystone.lb=0,0			#�����ı��ξ�����Ļ���½ǵ�x,y����
	persist.sys.keystone.rt=-50,0		#�����ı��ξ�����Ļ���Ͻǵ�x,y����
	persist.sys.keystone.rb=0,0			#�����ı��ξ�����Ļ���½ǵ�x,y����

	ע���谴�յѿ���ֱ������ϵ�����������ֵ����persist.sys.keystone.lt=50,-50��������Ļ����ʾ����persist.sys.keystone.lt=50,50���ᳬ����Ļ�Ϸ���

2��������ɺ��ٽ�persist.sys.keystone.update������ֵ����Ϊ1������ʹ������״��Ч��

3�����������������ޱ仯ʱ��������������������飺
	1��getprop vendor.hwc.compose_policy  �鿴����ֵ�Ƿ�Ϊ0����Ҫ��hwc�رա�
	2��getprop persist.sys.keystone.display.id  �鿴display id��ֵ�Ƿ�Ϊ��ǰ��Ҫ�����ε��豸��id�����ǲ�ƥ�䣬��setprop����Ϊ��Ӧ��id��

4�����ĳЩ�Ƕȹ��󣬵��µĻ��治���������¿������������Խӿڣ�
	1��setprop persist.sys.keystone.offset.x 0.0      ��ΧΪ:-1.0~1.0
	2��setprop persist.sys.keystone.offset.y 0.0      ��ΧΪ:-1.0~1.0

5��RockKeystoneӦ��
	��Ӧ���ṩ���ֶ���������У���Ĺ��ܣ����Һ�̨service�����gsensor���Զ���������У����
	Ӧ��Դ�����src/vendor/rockchip/common/apps/���ͻ��������н��ж��ο�����

3���汾����
