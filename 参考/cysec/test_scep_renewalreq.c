﻿#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <signal.h>
#include <cysec.h>
#include "test_util.h"

#if !(CYSEC_NO_SCEP) && !(CYSEC_NO_TLS)
static char* bin2hex(const unsigned char *old, const size_t oldlen)
{
    char *result = (char*) malloc(oldlen * 2 + 1);
    size_t i, j;
    int b = 0;

    for (i = j = 0; i < oldlen; i++) {
        b = old[i] >> 4;
        result[j++] = (char) (87 + b + (((b - 10) >> 31) & -39));
        b = old[i] & 0xf;
        result[j++] = (char) (87 + b + (((b - 10) >> 31) & -39));
    }
    result[j] = '\0';
    return result;

}

/** 
	scep客户端代码
	功能：构造客户端的更新证书请求句柄
	1.生成密钥对
	2.利用密钥对生成证书签发请求句柄
	3.为证书签发请求句柄设置参数项
	4.对证书签发请求句柄进行数字签名
	5.scep客户端根据证书签发请求句柄、客户端私钥、客户端证书构造SCEP客户端更新证书请求句柄
	6.对客户端更新证书请求句柄编码
 */
static void test_scep_renewalreq_request(void)
{
	const char* p[] = { "rsa", "sm2" };
	int ret = 0;
	unsigned int n = 0;

	for(n = 0; n < sizeof(p)/sizeof(char*); n ++){
		PKEY_PCTX new_pctx = NULL;
		X509REQ_PCTX x509req = NULL;
		X509CRT_PCTX old_crt = NULL;
		PKEY_PCTX old_pkey = NULL;
		X509CRT_PCTX scepsvr_crt = NULL;
		char path[256] = {0};
		unsigned char *req_pem = NULL, *req_der = NULL, *privatekey_pem = NULL;
		size_t plen = 0, rlen = 0, prikeypemlen = 0;
		SCEP_REQUEST_PCTX req = NULL;	
		const char *sn= "CN=7310500000000X_VIN_LSGBL5334HF000020,OU=China,O=SGM";
		DIGEST_PCTX dctx = NULL;
		unsigned char digest[128] = {0};
		char *digest_hex = NULL;

		//获取scep服务器证书（scep服务器可以看作是ca的一个代理，这一步就是要获取ca签发给scep服务器的身份证书）
		memset(path, 0, sizeof(path));
		snprintf(path, sizeof(path), "./kpool/%s.scep.scepsvr.pem", p[n]);
		scepsvr_crt = FILE_getcrt(path);
		s_assert((scepsvr_crt != NULL), "load certificate %s\n error", path);
		if(!scepsvr_crt){
			printf("Load certificate %s\n error",path);
			break;
		}	
		//scep客户端生成密钥对
		if( strcmp(p[n], "rsa") == 0 ){
			new_pctx = cysec_pkey_gen_rsa(1024);
			s_assert((new_pctx != NULL), "failure to generate rsa \n");
		}else if( strcmp(p[n], "sm2") == 0 ){
			new_pctx = cysec_pkey_gen_sm2();
			s_assert((new_pctx != NULL), "failure to generate sm2 \n");
		}
		
		if(!new_pctx)
			goto freebuffer;
		//从密钥中导出私钥
		ret = cysec_pkey_export_privatekey(new_pctx, &privatekey_pem, &prikeypemlen, PEM);
		s_assert((ret == 0), "export private key error(%08X).\n", ret);
		if(ret)
			goto freebuffer;
		//序列化私钥
		memset(path, 0, sizeof(path));
		snprintf(path, sizeof(path), "./kpool/%s.scep.new.pvk.pem", p[n]);		
		ret = FILE_putcontent(privatekey_pem, prikeypemlen, path);
		SAFE_FREE(privatekey_pem);
		//scep客户端利用私钥生成证书签发请求句柄
		x509req = cysec_x509req_new(new_pctx);
		s_assert((x509req!=NULL),"generate x509req error...\n");	
		if(!x509req)
			goto freebuffer;

		/*
		ret = cysec_x509req_set_subject_name(x509req, cysec_x509crt_get_subject(crt));
		s_assert((ret==0),"x509req set subject name error...%08x\n",ret);
		*/
		//为证书签发请求句柄设置主题项
		ret = cysec_x509req_set_subject_name(x509req, sn);
		s_assert((ret==0),"x509req set subject name error ...%08X\n",ret);
		if(ret)
			goto freebuffer;
		//为证书签发请求句柄设置序列号
		ret = cysec_x509req_set_serialnumber(x509req,"00:01:02:03");
		s_assert((ret == 0), "x509req set serialnumber error...%08x\n",ret);
		if(ret)
			goto freebuffer;
		//构造hash算法句柄，用于进行数字签名（密文通过此算法句柄生成哈希）
		dctx = cysec_digest_ctx_new(HASH_ALG_SHA256);
		s_assert((dctx!=NULL),"digest new error");
		if(!dctx)
			goto freebuffer;
		//算法句柄初始化
		ret = cysec_digest_init(dctx, NULL);
		s_assert((ret == 0), "digest init ,error = %08X\n",ret);
		if(ret)
			goto freebuffer;
		//对密文执行哈希运算，这里的密文即是 DQ5260C150107327
		ret = cysec_digest_update(dctx,(const unsigned char *)"DQ5260C150107327",
			strlen("DQ5260C150107327"));
		s_assert((ret == 0), "digest update, error = %08x\n", ret);
		if(ret)
			goto freebuffer;
		//获取哈希值
		ret = cysec_digest_final(dctx, digest);
		s_assert((ret == 0), "digest final, error = %08x\n", ret);
		if(ret)
			goto freebuffer;
		//哈希值转16进制形式
		digest_hex = bin2hex(digest, 32);
		//ret = cysec_x509req_set_altname(x509req, digest_hex, strlen(digest_hex));
		int j=0; for(j=0; j<32; j++) printf("%02X", digest[j]); printf("\n");
		//为证书签发请求句柄设置名字约束（这里使用哈希作为名字约束）
		ret = cysec_x509req_set_altname(x509req, digest, 32);
		//ret = cysec_x509req_set_altname(x509req,"DQ5260C150107327",strlen("DQ5260C150107327"));
		s_assert((ret == 0), "set altname ,error = %08X\n", ret);
		if(ret)
			goto freebuffer;
		//为证书签发请求句柄设置挑战项
		ret = cysec_x509req_set_challengepw(x509req, "password");
		s_assert((ret == 0),"x509req st challenge pw error...%08x\n",ret);
		if(ret)
			goto freebuffer;
		//打开证书签发请求，开启Subject Key ID 
		ret = cysec_x509req_enable_skid(x509req);
		s_assert((ret == 0), "x509req enable skid error...%08x\n",ret);
		if(ret)
			goto freebuffer;
		//scep客户端为 证书签发请求 进行数字签名 （应该是要用到私钥的）
		ret = cysec_x509req_sign(x509req);
		s_assert((ret == 0), "x509req signature error...%08x\n",ret);
		if(ret)
			goto freebuffer;
		//scep客户端导出证书签发请求
		ret = cysec_x509req_export(x509req, &req_pem, &plen, PEM);
		s_assert((ret == 0), "export x509req pem error ....%08x\n",ret);
		if(ret)
			goto freebuffer;
		printf("the (%s) csr is (%s)\n",p[n],(char *)req_pem);
		//序列化证书签发请求
		memset(path, 0, sizeof(path));
		snprintf(path, sizeof(path), "./kpool/%s.scep.new.req.pem", p[n]);		
		ret = FILE_putcontent(req_pem, plen, path);
		
		//获得scep客户端旧的证书签发请求（实际上就是旧证书，如果客户端需要更新证书，这里的旧证书需要一并发送给scep服务器）
		memset(path, 0, sizeof(path));
		snprintf(path, sizeof(path) ,"./kpool/%s.scep.crt.pem", p[n]);
		old_crt = FILE_getcrt(path);
		if(!old_crt) {
			printf("failed to get old crt from(%s)\n", path);
			goto freebuffer;
		}
		//获得scep客户端旧的密钥对
		memset(path, 0, sizeof(path));
		snprintf(path, sizeof(path) ,"./kpool/%s.scep.pvk.pem", p[n]);
		old_pkey = FILE_getpvk(path);
		if(!old_pkey) {
			printf("failed to get old pvk from(%s)\n", path);
			goto freebuffer;
		}
		//构造SCEP客户端更新证书请求句柄
		req = cysec_scep_request_renewalreq_new(x509req, old_crt, old_pkey, scepsvr_crt);
		s_assert((req!=NULL),"generate the scep request(renewalreq) error ..\n");
		if(!req)
			goto freebuffer;
		//对客户端更新证书请求句柄编码
		ret = cysec_scep_request_encode(req, &req_der, &rlen);
		s_assert((ret == 0), "scep encode error ..ret(%08X)\n",ret);
		if(ret)
			goto freebuffer;
		//固化此请求句柄
		memset(path, 0, sizeof(path));
		snprintf(path, sizeof(path), "./kpool/%s.scep.renewalreq.der", p[n]);		
		ret = FILE_putcontent(req_der,rlen, path);

		printf("generate and write renewalreq(%s) success.\n",p[n]);
freebuffer:
		SAFE_FREE(req_der);
		SAFE_FREE(req_pem);
		SAFE_FREE(privatekey_pem);
		SAFE_FREE(digest_hex);
		if(req)
			cysec_scep_request_free(req);
		SAFE_FREE(req_pem);
		if(new_pctx)
			cysec_pkey_free(new_pctx);
		if(old_crt)
			cysec_x509crt_free(old_crt);
		if(old_pkey)
			cysec_pkey_free(old_pkey);
		if(scepsvr_crt)
			cysec_x509crt_free(scepsvr_crt);
 		if(x509req)
			cysec_x509req_free(x509req);
		if(dctx)
			cysec_digest_ctx_free(dctx);
	}
}

/** for testing purpose. */
static int scep_verifysigner_cb(X509CRT_PCTX signer, void *userdata)
{
	CERTMGR_PCTX cm = (CERTMGR_PCTX)userdata;	
	int ret = 0;

	if(!signer || !userdata)
		return 0;
	
	ret = cysec_certmgr_verify(cm, signer);
	s_assert((ret == 0), "Verify Certificate Chain Failure, ret = %08x\n", ret);
	
	return (ret == 0) ? 1 : 0;
}

/**
	scep服务器端代码
	功能：scep服务器获得来自scep客户端的证书签发请求句柄之后，向ca服务器请求获取一张scep客户端的身份证书
	1.获取ca服务器证书
	2.获取scep客户端旧证书及scep客户端旧私钥（证书签发成功后，旧证书和旧私钥均被废弃）
	3.构造证书管理器，将ca服务器证书添加进证书管理器中
	4.获取scep客户端的更新证书请求句柄
	5.根据scep客户端旧证书和旧私钥，scep服务器构造对scep客户端的SCEP响应句柄
	6.验证ca证书
	7.解码来自scep客户端的更新证书请求句柄，根据解码后的信息对SCEP响应句柄进行一定处理
	8.scep服务器对响应句柄的响应类型和响应状态进行检查
	9.scep服务器签发证书给scep客户端
 */
static void test_scep_respond(void)
{
	const char* p[] = { "rsa", "sm2"};
	int ret = 0;
	unsigned int n = 0;

	for(n = 0; n < sizeof(p)/sizeof(char*); n ++){
		PKEY_PCTX local_pctx = NULL;
		X509CRT_PCTX local_crt = NULL;
		char path[256] = {0};
		unsigned char *pem = NULL;
		SCEP_RESPONSE_PCTX rsp = NULL;
		unsigned char *rsp_der = NULL;
		size_t rsp_dlen = 0;
		X509CRT_PCTX issuedcert = NULL;	
		CERTMGR_PCTX cm = NULL;
		X509CRT_PCTX cacert= NULL;

		//获取ca自签名证书
		memset(path, 0, sizeof(path));
		snprintf(path, sizeof(path), "./kpool/%s.scep.ca.crt.pem", p[n]);
		cacert = FILE_getcrt(path);
		s_assert((cacert != NULL), "load scep server certificate %s\n error", path);
		if(!cacert)
			goto freebuffer;	
		//获取远端scep客户端证书(请求)
		snprintf(path, sizeof(path), "./kpool/%s.scep.crt.pem", p[n]);
		snprintf(path, sizeof(path), "./kpool/%s.scep.req.pem", p[n]);
		local_crt = FILE_getcrt(path);
		s_assert((local_crt != NULL), "load local certificate %s\n error", path);
		if(!local_crt)
			goto freebuffer;
		//获取远端scep客户端私钥
		memset(path, 0, sizeof(path));
		snprintf(path, sizeof(path), "./kpool/%s.scep.pvk.pem", p[n]);
		local_pctx = FILE_getpvk(path);
		s_assert((local_pctx != NULL), "load local prviatekey %s\n error", path);
		if(!local_pctx)
			goto freebuffer;	
		//构造证书管理器
		cm = certmgr_new();
		if(!cm)
			goto freebuffer;
		//将ca服务器证书句柄加入管理器中
		if(cacert)
			ret = certmgr_add_ca(cm, cacert);
		s_assert((ret == 0), "ret=%d\n", ret);
		if(ret)
			goto freebuffer;
		//获取远端scep客户端更新证书请求句柄			///这里有问题，是否是scep客户端更新证书请求句柄存疑（应该不是）
		memset(path, 0, sizeof(path));
		snprintf(path, sizeof(path), "./kpool/%s.scep.renewalreq.certrep.der", p[n]);
		rsp_der = FILE_getcontent(path, &rsp_dlen);
		if(!rsp_der)
			goto freebuffer;
		//根据scep客户端旧证书和旧私钥，scep服务器构造对scep客户端的SCEP响应句柄
		rsp = cysec_scep_response_certrep_new(local_crt, local_pctx);
		s_assert("rsp!=NULL", "generate the scep response error ..\n");
		if(!rsp)
			goto freebuffer;
		//设置SCEP响应，验证签发者回调，这里应该就是验证ca证书链是否完整
		ret = cysec_scep_response_set_verifysigner_callback(rsp, scep_verifysigner_cb, (void *)cm);
		s_assert((ret == 0), "set verifysigner error \n");
		if(ret)
			goto freebuffer;
		//对SCEP响应句柄进行解码（应该是对scep客户端的更新证书请求句柄进行解码，之后scep响应句柄再对解码后的请求句柄做一定处理）
		ret = cysec_scep_response_decode(rsp_der, rsp_dlen, rsp);
		s_assert((ret == 0), "decode scep message error (%08X)", ret);
		if(ret)
			goto freebuffer;
		//获取scep响应类型
		ret = cysec_scep_response_get_messagetype(rsp);
		s_assert((ret == 3), "the messagetype(%d) is not expected",ret);
		if(ret)
			goto freebuffer;
		//获取SCEP响应状态
		ret = cysec_scep_response_get_pkistatus(rsp);
		s_assert((ret == 0), "the pkistatus is (%d)\n", ret);
		if(ret != 0) {
			ret = cysec_scep_response_get_failinfo(rsp);
			printf("the failinfo is %d\n", ret);
			goto freebuffer;
		}
		//获取SCEP签发出的证书（应该是ca签发的，scep是个转接站）
		issuedcert = cysec_scep_response_certrep_get_issuedcert(rsp);
		s_assert((issuedcert!=NULL),"fail to get issued certificate\n");

		if(issuedcert){
			printf("===================GetCert===========================\n");
			dumpcrt(issuedcert);
			printf("===================success===========================\n");
		}
		//对scep证书编码
		memset(path, 0, sizeof(path));
		snprintf(path, sizeof(path), "./kpool/%s.scep.new.crt.pem", p[n]);
		ret = FILE_putcontent((const unsigned char *)cysec_x509crt_as_pem(issuedcert), strlen(cysec_x509crt_as_pem(issuedcert)), path);
		if(ret)
			goto freebuffer;	

freebuffer:
		if(cm)
			certmgr_free(cm);
		if(issuedcert)
			cysec_x509crt_free(issuedcert);

		SAFE_FREE(pem);
		SAFE_FREE(rsp_der);
		if(local_pctx)
			cysec_pkey_free(local_pctx);
		if(local_crt)
			cysec_x509crt_free(local_crt);
		if(cacert)
			cysec_x509crt_free(cacert);
		if(rsp)
			cysec_scep_response_free(rsp);
	}
}

int main(void)
{
	test_scep_respond();
	test_scep_renewalreq_request();
	exit(0);
}

#else
int  main()
{
	return 0;
}

#endif //!(CYSEC_NO_SCEP) && !(CYSEC_NO_TLS)