�����Զ��滻
Linux���滻
 ./change_puk --teebin tee-pager.bin
������Զ�����һ��2048���ȵ�RSA��Կoem_privkey.pem�������ڵ�ǰĿ¼�£����Զ�ʹ�ø���Կ�еĹ�Կ�滻tee-pager.bin�е�ԭʼ��Կ
./change_puk --teebin tee-pager.bin --key oem_privkey.pem
ʹ�ÿͻ�ָ������Կ�еĹ�Կ���滻tee-pager.bin�е�ԭʼ��Կ����Կ������2048����



windows���滻
��change_public_key.exe���"����oem_privkey.pem"��ť�����ɲ�������Կ����ǰĿ¼
ѡ��ո����ɵ���Կ����Ҫ�޸ĵ�tee-pager.bin���񣬵��"�޸Ĺ�Կ"���Զ�ʹ����Կ�еĹ�Կ�滻tee-pager.bin�е�ԭʼ��Կ


ע�⣺
1.����change_public_key.exe�����BouncyCastle.Crypto.dll�������⣬��ȷ��BouncyCastle.Crypto.dll��change_public_key.exe��ͬһĿ¼�¡�
2.�����滻�Ĺ�Կ������OPTEE��֤TAӦ�úϷ��Եģ��û�����������Կ��Ҳ��Ҫ�滻optee_test/export-user_ta/keys/oem_privkey.pem�����±���TA
	����Կ�ڱ���TAʱ���TA���½���ǩ����