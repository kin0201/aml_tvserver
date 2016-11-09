#define LOG_TAG "tvserver"
#define LOG_TV_TAG "CTvScanner"

#include "CTvScanner.h"
#include "CTvChannel.h"
#include "CTvProgram.h"
#include "CTvRegion.h"
#include "CFrontEnd.h"

#include <tvconfig.h>


#define dvb_fend_para(_p) ((struct dvb_frontend_parameters*)(&_p))
#define IS_DVBT2_TS(_para) (_para.m_type == FE_OFDM && _para.terrestrial.para.u.ofdm.ofdm_mode == OFDM_DVBT2)
#define IS_ISDBT_TS(_para) (_para.m_type == FE_ISDBT)

CTvScanner *CTvScanner::mInstance;
CTvScanner::ScannerEvent CTvScanner::mCurEv;
CTvScanner::service_list_t CTvScanner::service_list_dummy;

CTvScanner *CTvScanner::getInstance()
{
    if (NULL == mInstance) mInstance = new CTvScanner();
    return mInstance;
}

CTvScanner::CTvScanner()
{
    mbScanStart = false;
    mpObserver = NULL;
    mSource = 0xff;
    mMinFreq = 1;
    mMaxFreq = 100;
    mCurScanStartFreq = 1;
    mCurScanEndFreq = 100;
}

CTvScanner::~CTvScanner()
{
    AM_EVT_Unsubscribe((long)mScanHandle, AM_SCAN_EVT_PROGRESS, evtCallback, NULL);
    AM_EVT_Unsubscribe((long)mScanHandle, AM_SCAN_EVT_SIGNAL, evtCallback, NULL);
}

int CTvScanner::Scan(char *feparas, char *scanparas) {
    CFrontEnd::FEParas fe(feparas);
    ScanParas sp(scanparas);
    return Scan(fe, sp);
}

int CTvScanner::Scan(CFrontEnd::FEParas &fp, ScanParas &sp) {
    stopScan();

    if (mbScanStart) {
        LOGW("scan is scanning, need first stop it");
        return -1;
    }

    AM_SCAN_CreatePara_t para;
    AM_DMX_OpenPara_t dmx_para;
    AM_SCAN_Handle_t handle = 0;
    int i;

    LOGD("Scan fe[%s] scan[%s]", fp.toString().c_str(), sp.toString().c_str());

    mFEParas = fp;
    mScanParas = sp;

    /*for convenient use*/
    mCurScanStartFreq = sp.getAtvFrequency1();
    mCurScanEndFreq = sp.getAtvFrequency2();

    //reset scanner status
    mCurEv.reset();
    mFEType = -1;

    // Create the scan
    memset(&para, 0, sizeof(para));
    para.fend_dev_id = 0;//default
    para.mode = sp.getMode();
    para.proc_mode = sp.getProc();
    if (createAtvParas(para.atv_para, fp, sp) != 0)
        return -1;
    if (createDtvParas(para.dtv_para, fp, sp) != 0) {
        freeAtvParas(para.atv_para);
        return -1;
    }
    const char *db_mode = config_get_str ( CFG_SECTION_TV, SYS_SCAN_TO_PRIVATE_DB, "false");
    if (!strcmp(db_mode, "true")) {
        para.store_cb = NULL;
    } else {
        para.store_cb = storeScanHelper;
    }

    // Start Scan
    memset(&dmx_para, 0, sizeof(dmx_para));
    AM_DMX_Open(para.dtv_para.dmx_dev_id, &dmx_para);
    if (AM_SCAN_Create(&para, &handle) != AM_SUCCESS) {
        LOGD("SCAN CREATE fail");
        handle = NULL;
    } else {
        AM_SCAN_Helper_t ts_type_helper;
        ts_type_helper.id = AM_SCAN_HELPER_ID_FE_TYPE_CHANGE;
        ts_type_helper.user = (void*)this;
        ts_type_helper.cb = FETypeHelperCBHelper;
        AM_SCAN_SetHelper(handle, &ts_type_helper);
        mScanHandle = handle;
        AM_SCAN_SetUserData(handle, (void *)this);
        AM_EVT_Subscribe((long)handle, AM_SCAN_EVT_PROGRESS, evtCallback, NULL);
        AM_EVT_Subscribe((long)handle, AM_SCAN_EVT_SIGNAL, evtCallback, NULL);
        if (AM_SCAN_Start(handle) != AM_SUCCESS) {
            AM_SCAN_Destroy(handle, AM_FALSE);
            AM_EVT_Unsubscribe((long)handle, AM_SCAN_EVT_PROGRESS, evtCallback, NULL);
            AM_EVT_Unsubscribe((long)handle, AM_SCAN_EVT_SIGNAL, evtCallback, NULL);
            handle = NULL;
        }
    }

    freeAtvParas(para.atv_para);
    freeDtvParas(para.dtv_para);

    if (handle == NULL) {
        return -1;
    }
    mbScanStart = true;//start call ok
    return 0;

}

int CTvScanner::pauseScan()
{
    LOGD("pauseScan scan started:%d", mbScanStart);
    if (mbScanStart) { //if start ok and not stop
        int ret = AM_SCAN_Pause(mScanHandle);
        LOGD("pauseScan , ret=%d", ret);
        return ret;
    }
    return 0;
}

int CTvScanner::resumeScan()
{
    LOGD("resumeScan scan started:%d", mbScanStart);
    if (mbScanStart) { //if start ok and not stop
        int ret = AM_SCAN_Resume(mScanHandle);
        LOGD("resumeScan , ret=%d", ret);
        return ret;
    }
    return 0;
}

int CTvScanner::stopScan()
{
    LOGD("StopScan is started:%d", mbScanStart);
    //requestExit();
    if (mbScanStart) { //if start ok and not stop
        int ret = AM_SCAN_Destroy(mScanHandle, AM_TRUE);
        AM_EVT_Unsubscribe((long)mScanHandle, AM_SCAN_EVT_PROGRESS, evtCallback, NULL);
        AM_EVT_Unsubscribe((long)mScanHandle, AM_SCAN_EVT_SIGNAL, evtCallback, NULL);
        AM_SEC_Cache_Reset(0);
        //stop loop
        mbScanStart = false;//stop ok
    }
    return 0;
}

int CTvScanner::getScanStatus(int *status)
{
    LOGD("getScanStatus scan started:%d", mbScanStart);
    if (mbScanStart && status) { //if start ok and not stop
        int ret = AM_SCAN_GetStatus(mScanHandle, status);
        LOGD("getScanStatus = [%d], ret=%d", *status, ret);
        return ret;
    }
    return 0;
}





int CTvScanner::insertLcnList(lcn_list_t &llist, ScannerLcnInfo *lcn, int idx)
{
    int found = 0;

    for (lcn_list_t::iterator p=llist.begin(); p != llist.end(); p++) {
        ScannerLcnInfo *pl = *p;
        //LOGD("list size:%d, pl:%#x", llist.size(), pl);

        if ((pl->net_id == lcn->net_id)
               && (pl->ts_id == lcn->ts_id)
               && (pl->service_id == lcn->service_id)) {
            pl->lcn[idx] = lcn->lcn[idx];
            pl->visible[idx] = lcn->visible[idx];
            pl->valid[idx] = lcn->valid[idx];
            found = 1;
        }
    }
    if (!found) {
        llist.push_back(lcn);
    }
    return found ? 1 : 0; //found = insert fail.
}

void CTvScanner::getLcnInfo(AM_SCAN_Result_t *result, AM_SCAN_TS_t *sts, lcn_list_t &llist)
{
    dvbpsi_nit_t *nits = ((sts->type == AM_SCAN_TS_ANALOG) || (result->start_para->dtv_para.standard == AM_SCAN_DTV_STD_ATSC)) ?
            NULL : sts->digital.nits;
    dvbpsi_nit_ts_t *ts;
    dvbpsi_descriptor_t *dr;
    dvbpsi_nit_t *nit;
    ScannerLcnInfo *plcninfo;

    UNUSED(result);

    AM_SI_LIST_BEGIN(nits, nit)
        AM_SI_LIST_BEGIN(nit->p_first_ts, ts)
            AM_SI_LIST_BEGIN(ts->p_first_descriptor, dr)
                if (dr->p_decoded && (dr->i_tag == AM_SI_DESCR_LCN_83)) {
                dvbpsi_logical_channel_number_83_dr_t *lcn_dr = (dvbpsi_logical_channel_number_83_dr_t*)dr->p_decoded;
                dvbpsi_logical_channel_number_83_t *lcn = lcn_dr->p_logical_channel_number;
                int j;
                for (j=0; j<lcn_dr->i_logical_channel_numbers_number; j++) {
                        plcninfo = (ScannerLcnInfo*)calloc(sizeof(ScannerLcnInfo),1);
                        plcninfo->net_id = ts->i_orig_network_id;
                        plcninfo->ts_id = ts->i_ts_id;
                        plcninfo->service_id = lcn->i_service_id;
                        plcninfo->lcn[0] = lcn->i_logical_channel_number;
                        plcninfo->visible[0] = lcn->i_visible_service_flag;
                        plcninfo->valid[0] = 1;
                        LOGD("sd lcn for service [%d:%d:%d] ---> l:%d v:%d",
                                plcninfo->net_id, plcninfo->ts_id, plcninfo->service_id,
                                plcninfo->lcn[0], plcninfo->visible[0]);
                        if (insertLcnList(llist, plcninfo, 0)) {
                            free(plcninfo);
                            LOGD("lcn exists 0.");
                        }
                        lcn++;
                    }
                } else if (dr->p_decoded && dr->i_tag==AM_SI_DESCR_LCN_88) {
                    dvbpsi_logical_channel_number_88_dr_t *lcn_dr = (dvbpsi_logical_channel_number_88_dr_t*)dr->p_decoded;
                    dvbpsi_logical_channel_number_88_t *lcn = lcn_dr->p_logical_channel_number;
                    int j;
                    for (j=0; j<lcn_dr->i_logical_channel_numbers_number; j++) {
                        plcninfo = (ScannerLcnInfo*)calloc(sizeof(ScannerLcnInfo), 1);
                        plcninfo->net_id = ts->i_orig_network_id;
                        plcninfo->ts_id = ts->i_ts_id;
                        plcninfo->service_id = lcn->i_service_id;
                        plcninfo->lcn[1] = lcn->i_logical_channel_number;
                        plcninfo->visible[1] = lcn->i_visible_service_flag;
                        plcninfo->valid[1] = 1;
                        LOGD("hd lcn for service [%d:%d:%d] ---> l:%d v:%d",
                               plcninfo->net_id, plcninfo->ts_id, plcninfo->service_id,
                               plcninfo->lcn[1], plcninfo->visible[1]);
                        if (insertLcnList(llist, plcninfo, 1)) {
                            free(plcninfo);
                            LOGD("lcn exists 1.");
                        }
                        lcn++;
                    }
                }
            AM_SI_LIST_END()
        AM_SI_LIST_END()
    AM_SI_LIST_END()
}

void CTvScanner::notifyLcn(ScannerLcnInfo *lcn)
{
    mCurEv.clear();
    mCurEv.mType = ScannerEvent::EVENT_LCN_INFO_DATA;
    mCurEv.mLcnInfo = *lcn;

    getInstance()->sendEvent(mCurEv);
}


void CTvScanner::notifyService(SCAN_ServiceInfo_t *srv)
{
    if (!srv->tsinfo) {
        LOGE("service with no tsinfo.");
        return;
    }

    mCurEv.reset();
    mCurEv.mFEParas = srv->tsinfo->fe;
    mCurEv.mFrequency = mCurEv.mFEParas.getFrequency();

    int feType = mCurEv.mFEParas.getFEMode().getBase();
    if (feType != FE_ANALOG) {
        mCurEv.mServiceId = srv->srv_id;
        mCurEv.mONetId = srv->tsinfo->nid;
        mCurEv.mTsId = srv->tsinfo->tsid;
        strncpy(mCurEv.mProgramName, srv->name, 1024);
        mCurEv.mprogramType = srv->srv_type;
        mCurEv.mVid = srv->vid;
        mCurEv.mVfmt = srv->vfmt;
        mCurEv.mAcnt = srv->aud_info.audio_count;
        for (int i = 0; i < srv->aud_info.audio_count; i++) {
            mCurEv.mAid[i] = srv->aud_info.audios[i].pid;
            mCurEv.mAfmt[i] = srv->aud_info.audios[i].fmt;
            strncpy(mCurEv.mAlang[i], srv->aud_info.audios[i].lang, 10);
            mCurEv.mAtype[i] = srv->aud_info.audios[i].audio_type;
            mCurEv.mAExt[i] = srv->aud_info.audios[i].audio_exten;
        }
        mCurEv.mPcr = srv->pcr_pid;

        if (srv->tsinfo->dtvstd == AM_SCAN_DTV_STD_ATSC) {
            mCurEv.mAccessControlled = srv->access_controlled;
            mCurEv.mHidden = srv->hidden;
            mCurEv.mHideGuide = srv->hide_guide;
            mCurEv.mSourceId = srv->source_id;
            mCurEv.mMajorChannelNumber = srv->major_chan_num;
            mCurEv.mMinorChannelNumber = srv->minor_chan_num;
        } else {
            mCurEv.mScnt = srv->sub_info.subtitle_count;
            for (int i = 0; i < srv->sub_info.subtitle_count; i++) {
                mCurEv.mStype[i] = TYPE_DVB_SUBTITLE;
                mCurEv.mSid[i] = srv->sub_info.subtitles[i].pid;
                mCurEv.mSstype[i] = srv->sub_info.subtitles[i].type;
                mCurEv.mSid1[i] = srv->sub_info.subtitles[i].comp_page_id;
                mCurEv.mSid2[i] = srv->sub_info.subtitles[i].anci_page_id;
                strncpy(mCurEv.mSlang[i], srv->sub_info.subtitles[i].lang, 10);
            }
            int scnt = mCurEv.mScnt;
            for (int i = 0; i < srv->ttx_info.teletext_count; i++) {
                if (srv->ttx_info.teletexts[i].type == 0x2 ||
                        srv->ttx_info.teletexts[i].type == 0x5) {
                    if (scnt >= (int)(sizeof(mCurEv.mStype) / sizeof(int)))
                        break;
                    mCurEv.mStype[scnt] = TYPE_DTV_TELETEXT;
                    mCurEv.mSid[scnt] = srv->ttx_info.teletexts[i].pid;
                    mCurEv.mSstype[scnt] = srv->ttx_info.teletexts[i].type;
                    mCurEv.mSid1[scnt] = srv->ttx_info.teletexts[i].magazine_no;
                    mCurEv.mSid2[scnt] = srv->ttx_info.teletexts[i].page_no;
                    strncpy(mCurEv.mSlang[scnt], srv->ttx_info.teletexts[i].lang, 10);
                    scnt++;
                }
            }
            mCurEv.mScnt = scnt;
            mCurEv.mFEParas.setPlp(srv->plp_id);
        }

        mCurEv.mFree_ca = srv->free_ca;
        mCurEv.mScrambled = srv->scrambled_flag;
        mCurEv.mSdtVer = srv->sdt_version;

        mCurEv.mType = ScannerEvent::EVENT_DTV_PROG_DATA;
        LOGD("notifyService freq:%d, sid:%d", mCurEv.mFrequency, srv->srv_id);

    } else {//analog

        mCurEv.mVideoStd = mCurEv.mFEParas.getVideoStd();
        mCurEv.mAudioStd = mCurEv.mFEParas.getAudioStd();
        mCurEv.mIsAutoStd = ((mCurEv.mVideoStd & V4L2_COLOR_STD_AUTO) == V4L2_COLOR_STD_AUTO) ? 1 : 0;

        mCurEv.mType = ScannerEvent::EVENT_ATV_PROG_DATA;
        LOGD("notifyService freq:%d, vstd:%x astd:%x",
            mCurEv.mFrequency, mCurEv.mFEParas.getVideoStd(), mCurEv.mFEParas.getAudioStd());
    }

    getInstance()->sendEvent(mCurEv);
}

void CTvScanner::sendEvent(ScannerEvent &evt)
{
    if (mpObserver) {
        if (evt.mType != ScannerEvent::EVENT_DTV_PROG_DATA) {
            evt.mAcnt = 0;//avoid invalid count confused the array.
            evt.mScnt = 0;
        }

        strncpy(mCurEv.mParas,
            mCurEv.mFEParas.toString().c_str(),
            sizeof(mCurEv.mParas));
        LOGD("FEParas:%s", mCurEv.mParas);

        mpObserver->onEvent(evt);
    }
}

void CTvScanner::processTsInfo(AM_SCAN_Result_t *result, AM_SCAN_TS_t *ts, SCAN_TsInfo_t *ts_info)
{
    dvbpsi_nit_t *nit;
    dvbpsi_descriptor_t *descr;

    ts_info->nid = -1;
    ts_info->tsid = -1;

    if (ts->type == AM_SCAN_TS_ANALOG) {
        CFrontEnd::FEMode mode(mFEParas.getFEMode());
        mode.setBase(FE_ANALOG);
        ts_info->fe.clear();
        ts_info->fe.setFEMode(mode).setFrequency(ts->analog.freq)
            .setVideoStd(CFrontEnd::stdAndColorToVideoEnum(ts->analog.std))
            .setAudioStd(CFrontEnd::stdAndColorToAudioEnum(ts->analog.std));
    } else {
        /*tsid*/
       dvbpsi_pat_t *pats = getValidPats(ts);
       if (pats != NULL && !ts->digital.use_vct_tsid) {
            ts_info->tsid = pats->i_ts_id;
            if (ts->digital.sdts)
                ts_info->tsid = ts->digital.sdts->i_ts_id;
            else if (IS_DVBT2_TS(ts->digital.fend_para) && ts->digital.dvbt2_data_plp_num > 0 && ts->digital.dvbt2_data_plps[0].sdts)
                ts_info->tsid = ts->digital.dvbt2_data_plps[0].sdts->i_ts_id;
        } else if (ts->digital.vcts != NULL) {
            ts_info->tsid = ts->digital.vcts->transport_stream_id;
        }

        /*nid*/
        if (result->start_para->dtv_para.standard != AM_SCAN_DTV_STD_ATSC ) {
            if (ts->digital.sdts)
                ts_info->nid = ts->digital.sdts->i_network_id;
            else if (IS_DVBT2_TS(ts->digital.fend_para) && ts->digital.dvbt2_data_plp_num > 0 && ts->digital.dvbt2_data_plps[0].sdts)
                ts_info->nid = ts->digital.dvbt2_data_plps[0].sdts->i_network_id;
        }

        CFrontEnd::FEMode mode(mFEParas.getFEMode());
        mode.setBase(ts->digital.fend_para.m_type);
        ts_info->fe.clear();
        ts_info->fe.fromFENDCTRLParameters(mode, &ts->digital.fend_para);
    }
    ts_info->dtvstd = result->start_para->dtv_para.standard;
}


dvbpsi_pat_t *CTvScanner::getValidPats(AM_SCAN_TS_t *ts)
{
    dvbpsi_pat_t *valid_pat = NULL;
    if (!IS_DVBT2_TS(ts->digital.fend_para)) {
        valid_pat = ts->digital.pats;
    } else if (IS_ISDBT_TS(ts->digital.fend_para)) {
        /* process for isdbt one-seg inserted PAT, which ts_id is 0xffff */
        valid_pat = ts->digital.pats;
        while (valid_pat != NULL && valid_pat->i_ts_id == 0xffff) {
            valid_pat = valid_pat->p_next;
        }

        if (valid_pat == NULL && ts->digital.pats != NULL) {
            valid_pat = ts->digital.pats;

            if (ts->digital.sdts != NULL)
                valid_pat->i_ts_id = ts->digital.sdts->i_ts_id;
        }
    } else {
        for (int plp = 0; plp < ts->digital.dvbt2_data_plp_num; plp++) {
            if (ts->digital.dvbt2_data_plps[plp].pats != NULL) {
                valid_pat = ts->digital.dvbt2_data_plps[plp].pats;
                break;
            }
        }
    }

    return valid_pat;
}

CTvScanner::SCAN_ServiceInfo_t* CTvScanner::getServiceInfo()
{
    SCAN_ServiceInfo_t *srv_info = (SCAN_ServiceInfo_t*)calloc(sizeof(SCAN_ServiceInfo_t), 1);
    if (!srv_info) {
       LOGE("No Memory for Scanner.");
       return NULL;
    }

    memset(srv_info, 0, sizeof(SCAN_ServiceInfo_t));
    srv_info->vid = 0x1fff;
    srv_info->vfmt = -1;
    srv_info->free_ca = 1;
    srv_info->srv_id = 0xffff;
    srv_info->pmt_pid = 0x1fff;
    srv_info->plp_id = -1;
    srv_info->sdt_version = 0xff;
    return srv_info;
}

int CTvScanner::getPmtPid(dvbpsi_pat_t *pats, int program_number)
{
    dvbpsi_pat_t *pat;
    dvbpsi_pat_program_t *prog;

    AM_SI_LIST_BEGIN(pats, pat)
    AM_SI_LIST_BEGIN(pat->p_first_program, prog)
    if (prog->i_number == program_number)
        return prog->i_pid;
    AM_SI_LIST_END()
    AM_SI_LIST_END()

    return 0x1fff;
}

void CTvScanner::extractCaScrambledFlag(dvbpsi_descriptor_t *p_first_descriptor, int *flag)
{
    dvbpsi_descriptor_t *descr;

    AM_SI_LIST_BEGIN(p_first_descriptor, descr)
    if (descr->i_tag == AM_SI_DESCR_CA && ! *flag) {
        LOGD( "Found CA descr, set scrambled flag to 1");
        *flag = 1;
        break;
    }
    AM_SI_LIST_END()
}

void CTvScanner::extractSrvInfoFromSdt(AM_SCAN_Result_t *result, dvbpsi_sdt_t *sdts, SCAN_ServiceInfo_t *srv_info)
{
    dvbpsi_sdt_service_t *srv;
    dvbpsi_sdt_t *sdt;
    dvbpsi_descriptor_t *descr;
    const uint8_t split = 0x80;
    const int name_size = (int)sizeof(srv_info->name);
    int curr_name_len = 0, tmp_len;
    char name[AM_DB_MAX_SRV_NAME_LEN + 1];

    UNUSED(result);

#define COPY_NAME(_s, _slen)\
    AM_MACRO_BEGIN\
    int copy_len = ((curr_name_len+_slen)>=name_size) ? (name_size-curr_name_len) : _slen;\
        if (copy_len > 0) {\
            memcpy(srv_info->name+curr_name_len, _s, copy_len);\
            curr_name_len += copy_len;\
        }\
    AM_MACRO_END


    AM_SI_LIST_BEGIN(sdts, sdt)
    AM_SI_LIST_BEGIN(sdt->p_first_service, srv)
    /*从SDT表中查找该service并获取信忿 */
    if (srv->i_service_id == srv_info->srv_id) {
        LOGD("SDT for service %d found!", srv_info->srv_id);
        srv_info->eit_sche = (uint8_t)srv->b_eit_schedule;
        srv_info->eit_pf = (uint8_t)srv->b_eit_present;
        srv_info->rs = srv->i_running_status;
        srv_info->free_ca = (uint8_t)srv->b_free_ca;
        srv_info->sdt_version = sdt->i_version;

        AM_SI_LIST_BEGIN(srv->p_first_descriptor, descr)
        if (descr->p_decoded && descr->i_tag == AM_SI_DESCR_SERVICE) {
            dvbpsi_service_dr_t *psd = (dvbpsi_service_dr_t *)descr->p_decoded;
            if (psd->i_service_name_length > 0) {
                name[0] = 0;
                AM_SI_ConvertDVBTextCode((char *)psd->i_service_name, psd->i_service_name_length, \
                                         name, AM_DB_MAX_SRV_NAME_LEN);
                name[AM_DB_MAX_SRV_NAME_LEN] = 0;
                LOGD("found name [%s]", name);

                /*3bytes language code, using xxx to simulate*/
                COPY_NAME("xxx", 3);
                /*following by name text*/
                tmp_len = strlen(name);
                COPY_NAME(name, tmp_len);
            }
            /*业务类型*/
            srv_info->srv_type = psd->i_service_type;
            /*service type 0x16 and 0x19 is user defined, as digital television service*/
            /*service type 0xc0 is type of partial reception service in ISDBT*/
            if ((srv_info->srv_type == 0x16) || (srv_info->srv_type == 0x19) || (srv_info->srv_type == 0xc0)) {
                srv_info->srv_type = 0x1;
            }
            break;
        }
        AM_SI_LIST_END()

        /* store multilingual service name */
        AM_SI_LIST_BEGIN(srv->p_first_descriptor, descr)
        if (descr->p_decoded && descr->i_tag == AM_SI_DESCR_MULTI_SERVICE_NAME) {
            int i;
            dvbpsi_multi_service_name_dr_t *pmsnd = (dvbpsi_multi_service_name_dr_t *)descr->p_decoded;

            for (i = 0; i < pmsnd->i_name_count; i++) {
                name[0] = 0;
                AM_SI_ConvertDVBTextCode((char *)pmsnd->p_service_name[i].i_service_name,
                                         pmsnd->p_service_name[i].i_service_name_length,
                                         name, AM_DB_MAX_SRV_NAME_LEN);
                name[AM_DB_MAX_SRV_NAME_LEN] = 0;
                LOGD("found name [%s]", name);

                if (curr_name_len > 0) {
                    /*extra split mark*/
                    COPY_NAME(&split, 1);
                }
                /*3bytes language code*/
                COPY_NAME(pmsnd->p_service_name[i].i_iso_639_code, 3);
                /*following by name text*/
                tmp_len = strlen(name);
                COPY_NAME(name, tmp_len);
            }
        }
        AM_SI_LIST_END()

        /* set the ending null byte */
        if (curr_name_len >= name_size)
            srv_info->name[name_size - 1] = 0;
        else
            srv_info->name[curr_name_len] = 0;

        break;
    }
    AM_SI_LIST_END()
    AM_SI_LIST_END()
}

void CTvScanner::extractSrvInfoFromVct(AM_SCAN_Result_t *result, vct_channel_info_t *vcinfo, SCAN_ServiceInfo_t *srv_info)
{
    UNUSED(result);

    srv_info->major_chan_num = vcinfo->major_channel_number;
    srv_info->minor_chan_num = vcinfo->minor_channel_number;

    srv_info->chan_num = (vcinfo->major_channel_number<<16) | (vcinfo->minor_channel_number&0xffff);
    srv_info->hidden = vcinfo->hidden;
    srv_info->hide_guide = vcinfo->hide_guide;
    srv_info->source_id = vcinfo->source_id;
    memcpy(srv_info->name, "xxx", 3);
    memcpy(srv_info->name+3, vcinfo->short_name, sizeof(vcinfo->short_name));
    srv_info->name[sizeof(vcinfo->short_name)+3] = 0;
    srv_info->srv_type = vcinfo->service_type;

    LOGD("Program(%d)('%s':%d-%d) in current TSID(%d) found!",
        srv_info->srv_id, srv_info->name,
        srv_info->major_chan_num, srv_info->minor_chan_num,
        vcinfo->channel_TSID);
}

void CTvScanner::updateServiceInfo(AM_SCAN_Result_t *result, SCAN_ServiceInfo_t *srv_info)
{
#define str(i) (char*)(strings + i)

    static char strings[14][256];

    if (srv_info->src != FE_ANALOG) {
        int standard = result->start_para->dtv_para.standard;
        int mode = result->start_para->dtv_para.mode;

        /* Transform service types for different dtv standards */
        if (standard != AM_SCAN_DTV_STD_ATSC) {
            if (srv_info->srv_type == 0x1)
                srv_info->srv_type = AM_SCAN_SRV_DTV;
            else if (srv_info->srv_type == 0x2)
                srv_info->srv_type = AM_SCAN_SRV_DRADIO;
        } else {
            if (srv_info->srv_type == 0x2)
                srv_info->srv_type = AM_SCAN_SRV_DTV;
            else if (srv_info->srv_type == 0x3)
                srv_info->srv_type = AM_SCAN_SRV_DRADIO;
        }

        /* if video valid, set this program to tv type,
         * if audio valid, but video not found, set it to radio type,
         * if both invalid, but service_type found in SDT/VCT, set to unknown service,
         * this mechanism is OPTIONAL
         */
        if (srv_info->vid < 0x1fff) {
            srv_info->srv_type = AM_SCAN_SRV_DTV;
        } else if (srv_info->aud_info.audio_count > 0) {
            srv_info->srv_type = AM_SCAN_SRV_DRADIO;
        } else if (srv_info->srv_type == AM_SCAN_SRV_DTV ||
                   srv_info->srv_type == AM_SCAN_SRV_DRADIO) {
            srv_info->srv_type = AM_SCAN_SRV_UNKNOWN;
        }
        /* Skip program for FTA mode */
        if (srv_info->scrambled_flag && (mode & AM_SCAN_DTVMODE_FTA)) {
            LOGD( "Skip program '%s' for FTA mode", srv_info->name);
            return;
        }

        /* Skip program for service_type mode */
        if (srv_info->srv_type == AM_SCAN_SRV_DTV && (mode & AM_SCAN_DTVMODE_NOTV)) {
            LOGD( "Skip program '%s' for NO-TV mode", srv_info->name);
            return;
        }
        if (srv_info->srv_type == AM_SCAN_SRV_DRADIO && (mode & AM_SCAN_DTVMODE_NORADIO)) {
            LOGD( "Skip program '%s' for NO-RADIO mode", srv_info->name);
            return;
        }

        /* Set default name to tv/radio program if no name specified */
        if (!strcmp(srv_info->name, "") &&
                (srv_info->srv_type == AM_SCAN_SRV_DTV ||
                 srv_info->srv_type == AM_SCAN_SRV_DRADIO)) {
            strcpy(srv_info->name, "xxxNo Name");
        }
    }
}

void CTvScanner::processDvbTs(AM_SCAN_Result_t *result, AM_SCAN_TS_t *ts, service_list_t &slist)
{
    LOGD("processDvbTs");

    dvbpsi_pmt_t *pmt;
    dvbpsi_pmt_es_t *es;
    int src = result->start_para->dtv_para.source;
    dvbpsi_pat_t *valid_pat = NULL;
    uint8_t plp_id;
    SCAN_ServiceInfo_t *psrv_info;

    valid_pat = getValidPats(ts);
    if (valid_pat == NULL) {
        LOGD("No PAT found in ts");
        return;
    }

    LOGD(" TS: src %d", src);

    if (ts->digital.pmts || (IS_DVBT2_TS(ts->digital.fend_para) && ts->digital.dvbt2_data_plp_num > 0)) {
        int loop_count, lc;
        dvbpsi_sdt_t *sdt_list;
        dvbpsi_pmt_t *pmt_list;
        dvbpsi_pat_t *pat_list;

        /* For DVB-T2, search for each PLP, else search in current TS*/
        loop_count = IS_DVBT2_TS(ts->digital.fend_para) ? ts->digital.dvbt2_data_plp_num : 1;
        LOGD("plp num %d", loop_count);

        for (lc = 0; lc < loop_count; lc++) {
            pat_list = IS_DVBT2_TS(ts->digital.fend_para) ? ts->digital.dvbt2_data_plps[lc].pats : ts->digital.pats;
            pmt_list = IS_DVBT2_TS(ts->digital.fend_para) ? ts->digital.dvbt2_data_plps[lc].pmts : ts->digital.pmts;
            sdt_list =  IS_DVBT2_TS(ts->digital.fend_para) ? ts->digital.dvbt2_data_plps[lc].sdts : ts->digital.sdts;
            plp_id = IS_DVBT2_TS(ts->digital.fend_para) ? ts->digital.dvbt2_data_plps[lc].id : -1;
            LOGD("plp_id %d", plp_id);

            AM_SI_LIST_BEGIN(pmt_list, pmt) {
                if (!(psrv_info = getServiceInfo()))
                    return;
                psrv_info->srv_id = pmt->i_program_number;
                psrv_info->src = src;
                psrv_info->pmt_pid = getPmtPid(pat_list, pmt->i_program_number);
                psrv_info->pcr_pid = pmt->i_pcr_pid;
                psrv_info->plp_id  = plp_id;

                /* looking for CA descr */
                if (! psrv_info->scrambled_flag) {
                    extractCaScrambledFlag(pmt->p_first_descriptor, &psrv_info->scrambled_flag);
                }

                AM_SI_LIST_BEGIN(pmt->p_first_es, es) {
                    AM_SI_ExtractAVFromES(es, &psrv_info->vid, &psrv_info->vfmt, &psrv_info->aud_info);
                    AM_SI_ExtractDVBSubtitleFromES(es, &psrv_info->sub_info);
                    AM_SI_ExtractDVBTeletextFromES(es, &psrv_info->ttx_info);
                    if (! psrv_info->scrambled_flag)
                        extractCaScrambledFlag(es->p_first_descriptor, &psrv_info->scrambled_flag);
                } AM_SI_LIST_END()

                extractSrvInfoFromSdt(result, sdt_list, psrv_info);

                /*Store this service*/
                updateServiceInfo(result, psrv_info);

                slist.push_back(psrv_info);
            } AM_SI_LIST_END()

            /* All programs in PMTs added, now trying the programs in SDT but NOT in PMT */
            dvbpsi_sdt_service_t *srv;
            dvbpsi_sdt_t *sdt;

            AM_SI_LIST_BEGIN(sdt_list, sdt) {
                AM_SI_LIST_BEGIN(sdt->p_first_service, srv) {
                    AM_Bool_t found_in_pmt = AM_FALSE;

                    /* Is already added in PMT? */
                    AM_SI_LIST_BEGIN(pmt_list, pmt){
                        if (srv->i_service_id == pmt->i_program_number) {
                            found_in_pmt = AM_TRUE;
                            break;
                        }
                    }AM_SI_LIST_END()

                    if (found_in_pmt)
                        continue;

                    if (!(psrv_info = getServiceInfo()))
                        return;
                    psrv_info->srv_id = srv->i_service_id;
                    psrv_info->src = src;

                    extractSrvInfoFromSdt(result, sdt_list, psrv_info);

                    updateServiceInfo(result, psrv_info);

                    /*as no pmt for this srv, set type to data for invisible*/
                    psrv_info->srv_type = 0;

                    slist.push_back(psrv_info);
                }
                AM_SI_LIST_END()
            }
            AM_SI_LIST_END()
        }
    }
}

void CTvScanner::processAnalogTs(AM_SCAN_Result_t *result, AM_SCAN_TS_t *ts, SCAN_TsInfo_t *tsinfo, service_list_t &slist)
{
    LOGD("processAnalogTs");

    SCAN_ServiceInfo_t *psrv_info;

    UNUSED(ts);

    if (!(psrv_info=getServiceInfo()))
        return;

    LOGD(" TS: src analog");

    psrv_info->tsinfo = tsinfo;

    /*if atsc, get analog major channel number from dtv info*/
    if (result->start_para->dtv_para.mode != AM_SCAN_DTVMODE_NONE
        && result->start_para->dtv_para.standard == AM_SCAN_DTV_STD_ATSC) {
        for (service_list_t::iterator p = slist.begin(); p != slist.end(); p++)
            if ((*p)->tsinfo->fe.getFrequency() == psrv_info->tsinfo->fe.getFrequency()) {
                psrv_info->major_chan_num = (*p)->major_chan_num;
                psrv_info->minor_chan_num = 0;
            }
    }

    slist.push_back(psrv_info);
}

void CTvScanner::processAtscTs(AM_SCAN_Result_t *result, AM_SCAN_TS_t *ts, service_list_t &slist)
{
    LOGD("processAtscTs");

    if(ts->digital.vcts
        && (ts->digital.vcts->transport_stream_id != ts->digital.pats->i_ts_id)
        && (ts->digital.pats->i_ts_id == 0))
        ts->digital.use_vct_tsid = 1;

    vct_section_info_t *vct;
    vct_channel_info_t *vcinfo;
    dvbpsi_pmt_t *pmt;
    dvbpsi_pmt_es_t *es;
    int src = result->start_para->dtv_para.source;
    AM_Bool_t stream_found_in_vct = AM_FALSE;
    AM_Bool_t program_found_in_vct = AM_FALSE;
    SCAN_ServiceInfo_t *psrv_info;

    if (!ts->digital.pats && !ts->digital.vcts)
    {
        LOGD("No PAT or VCT found in ts");
        return;
    }

    LOGD(" TS: src %d", src);

    AM_SI_LIST_BEGIN(ts->digital.pmts, pmt) {
        if (!(psrv_info = getServiceInfo()))
            return;
        psrv_info->srv_id = pmt->i_program_number;
        psrv_info->src = src;
        psrv_info->pmt_pid = getPmtPid(ts->digital.pats, pmt->i_program_number);
        psrv_info->pcr_pid = pmt->i_pcr_pid;

        /* looking for CA descr */
        if (! psrv_info->scrambled_flag) {
            extractCaScrambledFlag(pmt->p_first_descriptor, &psrv_info->scrambled_flag);
        }

        AM_SI_LIST_BEGIN(pmt->p_first_es, es) {
            AM_SI_ExtractAVFromES(es, &psrv_info->vid, &psrv_info->vfmt, &psrv_info->aud_info);
            if (! psrv_info->scrambled_flag)
                extractCaScrambledFlag(es->p_first_descriptor, &psrv_info->scrambled_flag);
        }AM_SI_LIST_END()

        program_found_in_vct = AM_FALSE;
        AM_SI_LIST_BEGIN(ts->digital.vcts, vct){
        AM_SI_LIST_BEGIN(vct->vct_chan_info, vcinfo){
            /*Skip inactive program*/
            if (vcinfo->program_number == 0  || vcinfo->program_number == 0xffff)
                continue;

            if ((ts->digital.use_vct_tsid || (vct->transport_stream_id == ts->digital.pats->i_ts_id))
                && vcinfo->channel_TSID == vct->transport_stream_id)
            {
                if (vcinfo->program_number == pmt->i_program_number)
                {
                    AM_SI_ExtractAVFromATSCVC(vcinfo, &psrv_info->vid, &psrv_info->vfmt, &psrv_info->aud_info);

                    extractSrvInfoFromVct(result, vcinfo, psrv_info);

                    program_found_in_vct = AM_TRUE;
                    goto VCT_END;
                }
            } else {
                AM_DEBUG(1, "Program(%d ts:%d) in VCT(ts:%d) found, current (ts:%d)",
                    vcinfo->program_number, vcinfo->channel_TSID,
                    vct->transport_stream_id, ts->digital.pats->i_ts_id);
                continue;
            }
        } AM_SI_LIST_END()
        } AM_SI_LIST_END()
VCT_END:
        /*Store this service*/
        updateServiceInfo(result, psrv_info);

        slist.push_back(psrv_info);

    } AM_SI_LIST_END()

    /* All programs in PMTs added, now trying the programs in VCT but NOT in PMT */
    AM_SI_LIST_BEGIN(ts->digital.vcts, vct) {
    AM_SI_LIST_BEGIN(vct->vct_chan_info, vcinfo) {
        AM_Bool_t found_in_pmt = AM_FALSE;

        if (!(psrv_info = getServiceInfo()))
            return;
        psrv_info->srv_id = vcinfo->program_number;
        psrv_info->src = src;

        /*Skip inactive program*/
        if (vcinfo->program_number == 0  || vcinfo->program_number == 0xffff)
            continue;

        /* Is already added in PMT? */
        AM_SI_LIST_BEGIN(ts->digital.pmts, pmt) {
            if (vcinfo->program_number == pmt->i_program_number) {
                found_in_pmt = AM_TRUE;
                break;
            }
        } AM_SI_LIST_END()

        if (found_in_pmt)
            continue;

        if (vcinfo->channel_TSID == vct->transport_stream_id) {
            AM_SI_ExtractAVFromATSCVC(vcinfo, &psrv_info->vid, &psrv_info->vfmt, &psrv_info->aud_info);
            extractSrvInfoFromVct(result, vcinfo, psrv_info);
            updateServiceInfo(result, psrv_info);

            slist.push_back(psrv_info);

        } else {
            AM_DEBUG(1, "Program(%d ts:%d) in VCT(ts:%d) found",
                vcinfo->program_number, vcinfo->channel_TSID,
                vct->transport_stream_id);
            continue;
        }
    } AM_SI_LIST_END()
    } AM_SI_LIST_END()

}

void CTvScanner::storeScanHelper(AM_SCAN_Result_t *result)
{
    if (mInstance)
        mInstance->storeScan(result, NULL);
    else
        LOGE("no Scanner running, ignore");
}

void CTvScanner::storeScan(AM_SCAN_Result_t *result, AM_SCAN_TS_t *curr_ts)
{
    AM_SCAN_TS_t *ts;
    service_list_t service_list;
    ts_list_t ts_list;

    LOGD("Storing tses ...");

    UNUSED(curr_ts);

    service_list_t slist;
    AM_SI_LIST_BEGIN(result->tses, ts) {
        if (ts->type == AM_SCAN_TS_DIGITAL) {
            SCAN_TsInfo_t *tsinfo = (SCAN_TsInfo_t*)calloc(sizeof(SCAN_TsInfo_t), 1);
            if (!tsinfo) {
                LOGE("No Memory for Scanner.");
                return;
            }
            processTsInfo(result, ts, tsinfo);
            ts_list.push_back(tsinfo);

            if (result->start_para->dtv_para.standard == AM_SCAN_DTV_STD_ATSC)
                processAtscTs(result, ts, slist);
            else
                processDvbTs(result, ts, slist);
            for (service_list_t::iterator p=slist.begin(); p != slist.end(); p++) {
                (*p)->tsinfo = tsinfo;
            }
            service_list.merge(slist);
            slist.clear();
        }
    }
    AM_SI_LIST_END()

    AM_SI_LIST_BEGIN(result->tses, ts) {
        if (ts->type == AM_SCAN_TS_ANALOG) {
            SCAN_TsInfo_t *tsinfo = (SCAN_TsInfo_t*)calloc(sizeof(SCAN_TsInfo_t), 1);
            if (!tsinfo) {
                LOGE("No memory for Scanner");
                return;
            }
            processTsInfo(result, ts, tsinfo);
            ts_list.push_back(tsinfo);

            service_list_t slist;
            processAnalogTs(result, ts, tsinfo, slist);
            service_list.merge(slist);
            slist.clear();
        }
    }
    AM_SI_LIST_END()

    if (result->start_para->dtv_para.sort_method == AM_SCAN_SORT_BY_LCN) {
        lcn_list_t lcn_list;
        AM_SI_LIST_BEGIN(result->tses, ts) {
            lcn_list_t llist;
            getLcnInfo(result, ts, llist);
            lcn_list.merge(llist);
            llist.clear();
        }
        AM_SI_LIST_END()

        /*notify lcn info*/
        LOGD("notify lcn info.");
        for (lcn_list_t::iterator p=lcn_list.begin(); p != lcn_list.end(); p++)
            notifyLcn(*p);

        /*free lcn list*/
        for (lcn_list_t::iterator p=lcn_list.begin(); p != lcn_list.end(); p++)
            free(*p);
        lcn_list.clear();
    }

    /*notify services info*/
    LOGD("notify service info.");
    for (service_list_t::iterator p=service_list.begin(); p != service_list.end(); p++)
        notifyService(*p);

    /*free services in list*/
    for (service_list_t::iterator p=service_list.begin(); p != service_list.end(); p++)
        free(*p);
    service_list.clear();

    /*free ts in list*/
    for (ts_list_t::iterator p=ts_list.begin(); p != ts_list.end(); p++)
        free(*p);
    ts_list.clear();
}


const char *CTvScanner::getDtvScanListName(int mode)
{
    char *list_name = NULL;
    CFrontEnd::FEMode feMode(mode);
    int base = feMode.getBase();
    int list = feMode.getList();

    switch (base) {
        case FE_DTMB:
            list_name = (char *)"CHINA,Default DTMB ALL";
            break;
        case FE_QAM:
            list_name = (char *)"China,DVB-C allband";
            break;
        case FE_OFDM:
            list_name = (char *)"UK,Default DVB-T";
            break;
        case FE_ATSC:
            if (list == 1)
                list_name = (char *)"U.S.,ATSC Cable Standard";
            else if (list == 2)
                list_name = (char *)"U.S.,ATSC Cable IRC";
            else if (list == 3)
                list_name = (char *)"U.S.,ATSC Cable HRC";
            else
                list_name = (char *)"U.S.,ATSC Air";
            break;
        default:
            list_name = (char *)"CHINA,Default DTMB ALL";
            LOGD("unknown scan mode %d, using default[%s]", mode, list_name);
            break;
    }
    return list_name;
}

AM_Bool_t CTvScanner::checkAtvCvbsLock(v4l2_std_id  *colorStd)
{
    tvafe_cvbs_video_t cvbs_lock_status;
    int ret, i = 0;

    *colorStd = 0;
    while (i < 20) {
        ret = CTvin::getInstance()->AFE_GetCVBSLockStatus(&cvbs_lock_status);

        if (cvbs_lock_status == TVAFE_CVBS_VIDEO_HV_LOCKED)
            /*||cvbs_lock_status == TVAFE_CVBS_VIDEO_V_LOCKED
            ||cvbs_lock_status == TVAFE_CVBS_VIDEO_H_LOCKED)*/ {
            usleep(2000 * 1000);
            tvin_info_t info;
            CTvin::getInstance()->VDIN_GetSignalInfo(&info);
            *colorStd = CTvin::CvbsFtmToV4l2ColorStd(info.fmt);
            LOGD("checkAtvCvbsLock locked and cvbs fmt = %d std = 0x%p", info.fmt, colorStd);
            return true;
        }
        usleep(50 * 1000);
        i++;
    }
    return false;
}

AM_Bool_t CTvScanner::checkAtvCvbsLockHelper(void *data)
{
    if (data == NULL) return false;
    AM_SCAN_ATV_LOCK_PARA_t *pAtvPara = (AM_SCAN_ATV_LOCK_PARA_t *)data;
    CTvScanner *pScan = (CTvScanner *)(pAtvPara->pData);
    if (mInstance != pScan)
        return AM_FALSE;
    v4l2_std_id std;
    AM_Bool_t isLock = pScan->checkAtvCvbsLock(&std);
    pAtvPara->pOutColorSTD = std;
    return isLock;
}


int CTvScanner::createAtvParas(AM_SCAN_ATVCreatePara_t &atv_para, CFrontEnd::FEParas &fp, ScanParas &sp) {
    atv_para.mode = sp.getAtvMode();
    if (atv_para.mode == AM_SCAN_ATVMODE_NONE)
        return 0;

    int freq1, freq2;
    freq1 = sp.getAtvFrequency1();
    freq2 = sp.getAtvFrequency2();
    if (freq1 <= 0 || freq2 <= 0
            || (atv_para.mode == AM_SCAN_ATVMODE_MANUAL && freq1 == freq2)
            || (atv_para.mode == AM_SCAN_ATVMODE_AUTO && freq1 > freq2)) {
        LOGW(" freq error start = %d end = %d",  freq1,  freq2);
        return -1;
    }

    atv_para.am_scan_atv_cvbs_lock =  &checkAtvCvbsLockHelper;
    atv_para.fe_paras = static_cast<AM_FENDCTRL_DVBFrontendParameters_t *>(calloc(3, sizeof(AM_FENDCTRL_DVBFrontendParameters_t)));
    if (atv_para.fe_paras != NULL) {
        memset(atv_para.fe_paras, 0, 3 * sizeof(AM_FENDCTRL_DVBFrontendParameters_t));
        atv_para.fe_paras[0].m_type = FE_ANALOG;
        atv_para.fe_paras[0].analog.para.frequency = sp.getAtvFrequency1();
        atv_para.fe_paras[1].m_type = FE_ANALOG;
        atv_para.fe_paras[1].analog.para.frequency = sp.getAtvFrequency2();
        atv_para.fe_paras[2].m_type = FE_ANALOG;
        atv_para.fe_paras[2].analog.para.frequency = (atv_para.mode == AM_SCAN_ATVMODE_AUTO)? 0 : sp.getAtvFrequency1();
    }
    atv_para.fe_cnt = 3;
    atv_para.default_std = CFrontEnd::getInstance()->enumToStdAndColor(fp.getVideoStd(), fp.getAudioStd());
    atv_para.channel_id = -1;
    atv_para.afc_range = 2000000;
    if (atv_para.mode == AM_SCAN_ATVMODE_AUTO) {
        atv_para.afc_unlocked_step = 3000000;
        atv_para.cvbs_unlocked_step = 1500000;
        atv_para.cvbs_locked_step = 6000000;
    } else {
        atv_para.direction = (atv_para.fe_paras[1].analog.para.frequency >= atv_para.fe_paras[0].analog.para.frequency)? 1 : 0;
        atv_para.cvbs_unlocked_step = 1000000;
        atv_para.cvbs_locked_step = 3000000;
    }
    return 0;
}
int CTvScanner::freeAtvParas(AM_SCAN_ATVCreatePara_t &atv_para) {
    if (atv_para.fe_paras != NULL)
        free(atv_para.fe_paras);
    return 0;
}

int CTvScanner::createDtvParas(AM_SCAN_DTVCreatePara_t &dtv_para, CFrontEnd::FEParas &fp, ScanParas &scp) {

    dtv_para.mode = scp.getDtvMode();
    if (dtv_para.mode == AM_SCAN_DTVMODE_NONE)
        return 0;

    dtv_para.source = fp.getFEMode().getBase();
    dtv_para.dmx_dev_id = 0;//default 0
    dtv_para.standard = AM_SCAN_DTV_STD_DVB;
    if (dtv_para.source == FE_ATSC)
        dtv_para.standard = AM_SCAN_DTV_STD_ATSC;
    else if (dtv_para.source == FE_ISDBT)
        dtv_para.standard = AM_SCAN_DTV_STD_ISDB;

    int forceDtvStd = scp.getDtvStandard();

    const char *dtvStd = config_get_str ( CFG_SECTION_TV, "dtv.scan.std.force", "null");
    if (!strcmp(dtvStd, "atsc"))
        forceDtvStd = AM_SCAN_DTV_STD_ATSC;
    else if (!strcmp(dtvStd, "dvb"))
        forceDtvStd = AM_SCAN_DTV_STD_DVB;
    else if (!strcmp(dtvStd, "isdb"))
        forceDtvStd = AM_SCAN_DTV_STD_ISDB;

    if (forceDtvStd != -1) {
        dtv_para.standard = (AM_SCAN_DTVStandard_t)forceDtvStd;
        LOGD("force dtv std: %d", forceDtvStd);
    }

    const char *list_name = getDtvScanListName(fp.getFEMode().getMode());
    LOGD("Using Region List [%s]", list_name);

    Vector<sp<CTvChannel>> vcp;

    if (scp.getDtvMode() == AM_SCAN_DTVMODE_ALLBAND) {
        CTvRegion::getChannelListByName((char *)list_name, vcp);
    } else {
        CTvRegion::getChannelListByNameAndFreqRange((char *)list_name, scp.getDtvFrequency1(), scp.getDtvFrequency2(), vcp);
    }
    int size = vcp.size();
    LOGD("channel list size = %d", size);

    if (size == 0) {
        if (scp.getMode() != AM_SCAN_DTVMODE_ALLBAND) {
            LOGD("frequncy: %d not found in channel list [%s], break", scp.getDtvFrequency1(), list_name);
            return -1;
        }
        CTvDatabase::GetTvDb()->importXmlToDB("/etc/tv_default.xml");
        CTvRegion::getChannelListByName((char *)list_name, vcp);
        size = vcp.size();
    }

    if (!(dtv_para.fe_paras = static_cast<AM_FENDCTRL_DVBFrontendParameters_t *>(calloc(size, sizeof(AM_FENDCTRL_DVBFrontendParameters_t)))))
        return -1;

    int i;
    for (i = 0; i < size; i++) {
        dtv_para.fe_paras[i].m_type = dtv_para.source;
        switch (dtv_para.fe_paras[i].m_type) {
            case FE_DTMB:
                dtv_para.fe_paras[i].dtmb.para.frequency = vcp[i]->getFrequency();
                dtv_para.fe_paras[i].dtmb.para.inversion = INVERSION_OFF;
                dtv_para.fe_paras[i].dtmb.para.u.ofdm.bandwidth = (fe_bandwidth_t)(vcp[i]->getBandwidth());
                if (fp.getBandwidth() != -1)
                    dtv_para.fe_paras[i].dtmb.para.u.ofdm.bandwidth = (fe_bandwidth_t)fp.getBandwidth();
                break;
            case FE_QAM:
                dtv_para.fe_paras[i].cable.para.frequency = vcp[i]->getFrequency();
                dtv_para.fe_paras[i].cable.para.inversion = INVERSION_OFF;
                dtv_para.fe_paras[i].cable.para.u.qam.symbol_rate = vcp[i]->getSymbolRate();
                dtv_para.fe_paras[i].cable.para.u.qam.modulation = (fe_modulation_t)vcp[i]->getModulation();
                if (fp.getSymbolrate() != -1)
                    dtv_para.fe_paras[i].cable.para.u.qam.symbol_rate = fp.getSymbolrate();
                if (fp.getModulation() != -1)
                    dtv_para.fe_paras[i].cable.para.u.qam.modulation = (fe_modulation_t)fp.getModulation();
                break;
            case FE_OFDM:
                dtv_para.fe_paras[i].terrestrial.para.frequency = vcp[i]->getFrequency();
                dtv_para.fe_paras[i].terrestrial.para.inversion = INVERSION_OFF;
                dtv_para.fe_paras[i].terrestrial.para.u.ofdm.bandwidth = (fe_bandwidth_t)(vcp[i]->getBandwidth());
                dtv_para.fe_paras[i].terrestrial.para.u.ofdm.ofdm_mode = (fe_ofdm_mode_t)fp.getFEMode().getGen();
                if (fp.getBandwidth() != -1)
                    dtv_para.fe_paras[i].terrestrial.para.u.ofdm.bandwidth = (fe_bandwidth_t)fp.getBandwidth();
                break;
            case FE_ATSC:
                dtv_para.fe_paras[i].atsc.para.frequency = vcp[i]->getFrequency();
                dtv_para.fe_paras[i].atsc.para.inversion = INVERSION_OFF;
                dtv_para.fe_paras[i].atsc.para.u.vsb.modulation = (fe_modulation_t)(vcp[i]->getModulation());
                if (fp.getModulation() != -1)
                    dtv_para.fe_paras[i].atsc.para.u.vsb.modulation = (fe_modulation_t)(fe_modulation_t)fp.getModulation();
                break;
        }
    }

    dtv_para.fe_cnt = size;
    dtv_para.resort_all = AM_FALSE;

    const char *sort_mode = config_get_str ( CFG_SECTION_TV, CFG_DTV_SCAN_SORT_MODE, "null");
    if (!strcmp(sort_mode, "lcn") && (dtv_para.standard != AM_SCAN_DTV_STD_ATSC))
        dtv_para.sort_method = AM_SCAN_SORT_BY_LCN;
    else
        dtv_para.sort_method = AM_SCAN_SORT_BY_FREQ_SRV_ID;

    return 0;
}
int CTvScanner::freeDtvParas(AM_SCAN_DTVCreatePara_t &dtv_para) {
    if (dtv_para.fe_paras != NULL)
        free(dtv_para.fe_paras);
    return 0;
}

void CTvScanner::reconnectDmxToFend(int dmx_no, int fend_no)
{
    AM_DMX_Source_t src;

    if (AM_FEND_GetTSSource(fend_no, &src) == AM_SUCCESS) {
        LOGD("Set demux%d source to %d", dmx_no, src);
        AM_DMX_SetSource(dmx_no, src);
    } else {
        LOGD("Cannot get frontend ts source!!");
    }
}

int CTvScanner::FETypeHelperCBHelper(int id, void *para, void *user) {
    if (mInstance)
        mInstance->FETypeHelperCB(id, para, user);
    else
        LOGE("no scanner running, ignore");
    return -1;
}

int CTvScanner::FETypeHelperCB(int id, void *para, void *user) {
    if ((id != AM_SCAN_HELPER_ID_FE_TYPE_CHANGE)
        || (user != (void*)this))
        return -1;

    fe_type_t type = static_cast<fe_type_t>(reinterpret_cast<intptr_t>(para));
    LOGD("FE set mode %d", type);

    if (type == mFEType)
        return 0;

    mFEType = type;

    CFrontEnd *fe = CFrontEnd::getInstance();
    CTvin *tvin = CTvin::getInstance();
    if (type == FE_ANALOG) {
        fe->setMode(type);
        //tvin->AFE_OpenModule();
        //tvin->VDIN_OpenModule();
        tvin->VDIN_ClosePort();
        tvin->VDIN_OpenPort(tvin->Tvin_GetSourcePortBySourceInput(SOURCE_TV));
    } else {
        tvin->VDIN_ClosePort();
        //tvin->VDIN_OpenPort(tvin->Tvin_GetSourcePortBySourceInput(SOURCE_DTV));
        //tvin->VDIN_CloseModule();
        //tvin->AFE_CloseModule();
        fe->setMode(type);
        //CTvScanner *pScanner = (CTvScanner *)user;
        reconnectDmxToFend(0, 0);
    }
    return 0;
}


 void CTvScanner::evtCallback(long dev_no, int event_type, void *param, void *data __unused)
{
    CTvScanner *pT = NULL;
    long long tmpFreq = 0;

    LOGD("evt evt:%d", event_type);
    AM_SCAN_GetUserData((AM_SCAN_Handle_t)dev_no, (void **)&pT);
    if (pT == NULL) {
        return;
    }
    int AdtvMixed = (pT->mScanParas.getAtvMode() != AM_SCAN_ATVMODE_NONE
        && pT->mScanParas.getDtvMode() != AM_SCAN_DTVMODE_NONE)? 1 : 0;
    int factor =  (AdtvMixed)? 50 : 100;

    pT->mCurEv.clear();
    memset(pT->mCurEv.mProgramName, '\0', sizeof(pT->mCurEv.mProgramName));
    memset(pT->mCurEv.mParas, '\0', sizeof(pT->mCurEv.mParas));
    if (event_type == AM_SCAN_EVT_PROGRESS) {
        AM_SCAN_Progress_t *evt = (AM_SCAN_Progress_t *)param;
        LOGD("progress evt:%d", evt->evt);
        switch (evt->evt) {
        case AM_SCAN_PROGRESS_SCAN_BEGIN: {
            AM_SCAN_CreatePara_t *cp = (AM_SCAN_CreatePara_t*)evt->data;
            pT->mCurEv.mPercent = 0;
            pT->mCurEv.mScanMode = (cp->mode<<24)|((cp->atv_para.mode&0xFF)<<16)|(cp->dtv_para.mode&0xFFFF);
            pT->mCurEv.mSortMode = (cp->dtv_para.standard<<16)|(cp->dtv_para.sort_method&0xFFFF);
            pT->mCurEv.mType = ScannerEvent::EVENT_SCAN_BEGIN;
            pT->sendEvent(pT->mCurEv);
            }
            break;
        case AM_SCAN_PROGRESS_NIT_BEGIN:
            break;
        case AM_SCAN_PROGRESS_NIT_END:
            break;
        case AM_SCAN_PROGRESS_TS_BEGIN: {
            AM_SCAN_TSProgress_t *tp = (AM_SCAN_TSProgress_t *)evt->data;
            if (tp == NULL)
                break;
            pT->mCurEv.mChannelIndex = tp->index;
            if (pT->mFEParas.getFEMode().getBase() == tp->fend_para.m_type)
                pT->mCurEv.mMode = pT->mFEParas.getFEMode().getMode();
            else
                pT->mCurEv.mMode = tp->fend_para.m_type;
            pT->mCurEv.mFrequency = ((struct dvb_frontend_parameters *)(&tp->fend_para))->frequency;
            pT->mCurEv.mSymbolRate = tp->fend_para.cable.para.u.qam.symbol_rate;
            pT->mCurEv.mModulation = tp->fend_para.cable.para.u.qam.modulation;
            pT->mCurEv.mBandwidth = tp->fend_para.terrestrial.para.u.ofdm.bandwidth;

            pT->mCurEv.mFEParas.fromFENDCTRLParameters(CFrontEnd::FEMode(pT->mCurEv.mMode), &tp->fend_para);


            if (tp->fend_para.m_type == FE_ANALOG) {
                if (pT->mFEParas.getFEMode().getExt() & 1) {//mix
                    pT->mCurEv.mPercent = (tp->index * factor) / tp->total;
                } else {
                    pT->mCurEv.mPercent = 0;
                }
            } else {
                pT->mCurEv.mPercent = (tp->index * factor) / tp->total;
            }

            if (pT->mCurEv.mTotalChannelCount == 0)
                pT->mCurEv.mTotalChannelCount = tp->total;
            if (pT->mCurEv.mPercent >= factor)
                pT->mCurEv.mPercent = factor -1;

            pT->mCurEv.mLockedStatus = 0;
            pT->mCurEv.mStrength = 0;
            pT->mCurEv.mSnr = 0;
            pT->mCurEv.mType = ScannerEvent::EVENT_SCAN_PROGRESS;

            pT->sendEvent(pT->mCurEv);
        }
        break;
        case AM_SCAN_PROGRESS_TS_END: {
            /*pT->mCurEv.mLockedStatus = 0;
            pT->mCurEv.mType = ScannerEvent::EVENT_SCAN_PROGRESS;
            pT->sendEvent(pT->mCurEv);*/
        }
        break;

        case AM_SCAN_PROGRESS_PAT_DONE: /*{
                if (pT->mCurEv.mTotalChannelCount == 1) {
                    pT->mCurEv.mType = ScannerEvent::EVENT_SCAN_PROGRESS;
                    pT->sendEvent(pT->mCurEv);
                }
            }*/
            break;
        case AM_SCAN_PROGRESS_SDT_DONE: /*{
                dvbpsi_sdt_t *sdts = (dvbpsi_sdt_t *)evt->data;
                dvbpsi_sdt_t *sdt;

                if (pT->mCurEv.mTotalChannelCount == 1) {
                    pT->mCurEv.mPercent += 25;
                    if (pT->mCurEv.mPercent >= 100)
                        pT->mCurEv.mPercent = 99;
                    pT->mCurEv.mType = ScannerEvent::EVENT_SCAN_PROGRESS;

                    pT->sendEvent(pT->mCurEv);
                }
            }*/
            break;
        case AM_SCAN_PROGRESS_CAT_DONE: /*{
                dvbpsi_cat_t *cat = (dvbpsi_cat_t *)evt->data;
                if (pT->mCurEv.mTotalChannelCount == 1) {
                    pT->mCurEv.mPercent += 25;
                    if (pT->mCurEv.mPercent >= 100)
                        pT->mCurEv.mPercent = 99;

                    pT->mCurEv.mType = ScannerEvent::EVENT_SCAN_PROGRESS;

                    pT->sendEvent(pT->mCurEv);
                }
            }*/
            break;
        case AM_SCAN_PROGRESS_PMT_DONE: /*{
                dvbpsi_pmt_t *pmt = (dvbpsi_pmt_t *)evt->data;
                if (pT->mCurEv.mTotalChannelCount == 1) {
                    pT->mCurEv.mPercent += 25;
                    if (pT->mCurEv.mPercent >= 100)
                        pT->mCurEv.mPercent = 99;

                    pT->mCurEv.mType = ScannerEvent::EVENT_SCAN_PROGRESS;
                    pT->sendEvent(pT->mCurEv);
                }
            }*/
            break;
        case AM_SCAN_PROGRESS_MGT_DONE: {
            mgt_section_info_t *mgt = (mgt_section_info_t *)evt->data;

            if (pT->mCurEv.mTotalChannelCount == 1) {
                pT->mCurEv.mPercent += 10;
                if (pT->mCurEv.mPercent >= 100)
                    pT->mCurEv.mPercent = 99;

                pT->mCurEv.mType = ScannerEvent::EVENT_SCAN_PROGRESS;
                pT->sendEvent(pT->mCurEv);
            }
        }
        break;
        case AM_SCAN_PROGRESS_VCT_DONE: {
            /*ATSC TVCT*/
            if (pT->mCurEv.mTotalChannelCount == 1) {
                pT->mCurEv.mPercent += 30;
                if (pT->mCurEv.mPercent >= 100)
                    pT->mCurEv.mPercent = 99;
                pT->mCurEv.mType = ScannerEvent::EVENT_SCAN_PROGRESS;
                pT->sendEvent(pT->mCurEv);
            }
        }
        break;
        case AM_SCAN_PROGRESS_NEW_PROGRAM: {
            /* Notify the new searched programs */
            AM_SCAN_ProgramProgress_t *pp = (AM_SCAN_ProgramProgress_t *)evt->data;
            if (pp != NULL) {
                pT->mCurEv.mprogramType = pp->service_type;
                snprintf(pT->mCurEv.mProgramName, sizeof(pT->mCurEv.mProgramName), "%s", pp->name);
                pT->mCurEv.mType = ScannerEvent::EVENT_SCAN_PROGRESS;
                pT->sendEvent(pT->mCurEv);
            }
        }
        break;
        case AM_SCAN_PROGRESS_NEW_PROGRAM_MORE: {
            AM_SCAN_NewProgram_Data_t *npd = (AM_SCAN_NewProgram_Data_t *)evt->data;
            if (npd != NULL) {
                switch (npd->newts->type) {
                    case AM_SCAN_TS_ANALOG:
                        if (npd->result->start_para->atv_para.mode == AM_SCAN_ATVMODE_AUTO)
                            pT->storeScan(npd->result, npd->newts);
                    break;
                    case AM_SCAN_TS_DIGITAL:
                        if ((npd->result->start_para->dtv_para.mode == AM_SCAN_DTVMODE_AUTO)
                            || (npd->result->start_para->dtv_para.mode == AM_SCAN_DTVMODE_ALLBAND))
                            pT->storeScan(npd->result, npd->newts);
                    break;
                    default:
                    break;
                }
            }
        }
        break;
        case AM_SCAN_PROGRESS_BLIND_SCAN: {
            AM_SCAN_DTVBlindScanProgress_t *bs_prog = (AM_SCAN_DTVBlindScanProgress_t *)evt->data;

            if (bs_prog) {
                pT->mCurEv.mPercent = bs_prog->progress;

                /*snprintf(pT->mCurEv.mMSG, sizeof(pT->mCurEv.mMSG), "%s/%s %dMHz",
                         bs_prog->polar == AM_FEND_POLARISATION_H ? "H" : "V",
                         bs_prog->lo == AM_FEND_LOCALOSCILLATORFREQ_L ? "L-LOF" : "H-LOF",
                         bs_prog->freq / 1000);
                */
                pT->mCurEv.mType = ScannerEvent::EVENT_BLINDSCAN_PROGRESS;

                pT->sendEvent(pT->mCurEv);

                if (bs_prog->new_tp_cnt > 0) {
                    int i = 0;
                    for (i = 0; i < bs_prog->new_tp_cnt; i++) {
                        LOGD("New tp: %dkS/s %d====", bs_prog->new_tps[i].frequency,
                             bs_prog->new_tps[i].u.qpsk.symbol_rate);

                        pT->mCurEv.mFrequency = bs_prog->new_tps[i].frequency;
                        pT->mCurEv.mSymbolRate = bs_prog->new_tps[i].u.qpsk.symbol_rate;
                        pT->mCurEv.mSat_polarisation = bs_prog->polar;
                        pT->mCurEv.mType = ScannerEvent::EVENT_BLINDSCAN_NEWCHANNEL;
                        pT->sendEvent(pT->mCurEv);
                    }
                }
                if (bs_prog->progress >= 100) {
                    pT->mCurEv.mType = ScannerEvent::EVENT_BLINDSCAN_END;
                    pT->sendEvent(pT->mCurEv);
                    pT->mCurEv.mPercent = 0;
                }
            }
        }
        break;
        case AM_SCAN_PROGRESS_STORE_BEGIN: {
            pT->mCurEv.mType = ScannerEvent::EVENT_STORE_BEGIN;
            pT->mCurEv.mLockedStatus = 0;
            pT->sendEvent(pT->mCurEv);
        }
        break;
        case AM_SCAN_PROGRESS_STORE_END: {
            pT->mCurEv.mLockedStatus = 0;
            pT->mCurEv.mType = ScannerEvent::EVENT_STORE_END;
            pT->sendEvent(pT->mCurEv);
        }
        break;
        case AM_SCAN_PROGRESS_SCAN_END: {
            pT->mCurEv.mPercent = 100;
            pT->mCurEv.mLockedStatus = 0;
            pT->mCurEv.mType = ScannerEvent::EVENT_SCAN_END;
            pT->sendEvent(pT->mCurEv);
        }
        break;
        case AM_SCAN_PROGRESS_SCAN_EXIT: {
            pT->mCurEv.mLockedStatus = 0;
            pT->mCurEv.mType = ScannerEvent::EVENT_SCAN_EXIT;
            pT->sendEvent(pT->mCurEv);
        }
        break;
        case AM_SCAN_PROGRESS_ATV_TUNING: {
            CFrontEnd::FEMode femode(FE_ANALOG);
            pT->mCurEv.mFEParas.setFEMode(femode);
            pT->mCurEv.mFEParas.setFrequency((int)evt->data);
            pT->mCurEv.mFrequency = (int)evt->data;
            pT->mCurEv.mLockedStatus = 0;
            tmpFreq = (pT->mCurEv.mFrequency - pT->mCurScanStartFreq) / 1000000;
            pT->mCurEv.mPercent = tmpFreq * factor / ((pT->mCurScanEndFreq - pT->mCurScanStartFreq) / 1000000)
                                                + ((factor == 50)? 50 : 0);
            pT->mCurEv.mType = ScannerEvent::EVENT_SCAN_PROGRESS;

            pT->sendEvent(pT->mCurEv);
        }
        break;

        default:
            break;
        }
    } else if (event_type == AM_SCAN_EVT_SIGNAL) {
        AM_SCAN_DTVSignalInfo_t *evt = (AM_SCAN_DTVSignalInfo_t *)param;
        //pT->mCurEv.mprogramType = 0xff;
        pT->mCurEv.mFrequency = (int)evt->frequency;
        pT->mCurEv.mFEParas.setFrequency(evt->frequency);
        pT->mCurEv.mLockedStatus = (evt->locked ? 1 : 0);

        if (pT->mCurEv.mFEParas.getFEMode().getBase() == FE_ANALOG && evt->locked)//trick here for atv new prog
            pT->mCurEv.mLockedStatus |= 0x10;

        pT->mCurEv.mType = ScannerEvent::EVENT_SCAN_PROGRESS;
        if (pT->mCurEv.mFEParas.getFEMode().getBase() != FE_ANALOG && evt->locked) {
            pT->mCurEv.mStrength = evt->strength;
            pT->mCurEv.mSnr = evt->snr;
        } else {
            pT->mCurEv.mStrength = 0;
            pT->mCurEv.mSnr = 0;
        }

        //if (pT->mCurEv.mMode == FE_ANALOG)
        pT->sendEvent(pT->mCurEv);
        pT->mCurEv.mLockedStatus &= ~0x10;
    }
}


const char* CTvScanner::ScanParas::SCP_MODE = "m";
const char* CTvScanner::ScanParas::SCP_ATVMODE = "am";
const char* CTvScanner::ScanParas::SCP_DTVMODE = "dm";
const char* CTvScanner::ScanParas::SCP_ATVFREQ1 = "af1";
const char* CTvScanner::ScanParas::SCP_ATVFREQ2 = "af2";
const char* CTvScanner::ScanParas::SCP_DTVFREQ1 = "df1";
const char* CTvScanner::ScanParas::SCP_DTVFREQ2 = "df2";
const char* CTvScanner::ScanParas::SCP_PROC = "prc";
const char* CTvScanner::ScanParas::SCP_DTVSTD = "dstd";

CTvScanner::ScanParas& CTvScanner::ScanParas::operator = (const ScanParas &spp)
{
    this->mparas = spp.mparas;
    return *this;
}


int CTvScanner::getAtscChannelPara(int attennaType, Vector<sp<CTvChannel> > &vcp)
{
    switch (attennaType) { //region name should be remove to config file and read here
    case 1:
        CTvRegion::getChannelListByName((char *)"U.S.,ATSC Air", vcp);
        break;
    case 2:
        CTvRegion::getChannelListByName((char *)"U.S.,ATSC Cable Standard", vcp);
        break;
    case 3:
        CTvRegion::getChannelListByName((char *)"U.S.,ATSC Cable IRC", vcp);
        break;
    case 4:
        CTvRegion::getChannelListByName((char *)"U.S.,ATSC Cable HRC", vcp);
        break;
    default:
        return -1;
    }

    return 0;
}

int CTvScanner::ATVManualScan(int min_freq, int max_freq, int std, int store_Type, int channel_num)
{
    UNUSED(store_Type);
    UNUSED(channel_num);

    char paras[128];
    CFrontEnd::convertParas(paras, FE_ANALOG, min_freq, max_freq, std, 0);

    CFrontEnd::FEParas fe(paras);

    ScanParas sp;
    sp.setMode(AM_SCAN_MODE_ATV_DTV);
    sp.setDtvMode(AM_SCAN_DTVMODE_NONE);
    sp.setAtvMode(AM_SCAN_ATVMODE_MANUAL);
    sp.setAtvFrequency1(min_freq);
    sp.setAtvFrequency2(max_freq);
    return Scan(fe, sp);
}

int CTvScanner::autoAtvScan(int min_freq, int max_freq, int std, int search_type, int proc_mode)
{
    UNUSED(search_type);

    char paras[128];
    CFrontEnd::convertParas(paras, FE_ANALOG, min_freq, max_freq, std, 0);

    CFrontEnd::FEParas fe(paras);

    ScanParas sp;
    sp.setMode(AM_SCAN_MODE_ATV_DTV);
    sp.setDtvMode(AM_SCAN_DTVMODE_NONE);
    sp.setAtvMode(AM_SCAN_ATVMODE_AUTO);
    sp.setAtvFrequency1(min_freq);
    sp.setAtvFrequency2(max_freq);
    sp.setProc(proc_mode);
    return Scan(fe, sp);
}

int CTvScanner::autoAtvScan(int min_freq, int max_freq, int std, int search_type)
{
    return autoAtvScan(min_freq, max_freq, std, search_type, 0);
}

int CTvScanner::dtvScan(int mode, int scan_mode, int beginFreq, int endFreq, int para1, int para2)
{
    char feparas[128];
    CFrontEnd::convertParas(feparas, mode, beginFreq, endFreq, para1, para2);
    return dtvScan(feparas, scan_mode, beginFreq, endFreq);
}
int CTvScanner::dtvScan(char *feparas, int scan_mode, int beginFreq, int endFreq)
{
    LOGE("dtvScan fe:[%s]", feparas);
    CFrontEnd::FEParas fe(feparas);
    ScanParas sp;
    sp.setMode(AM_SCAN_MODE_DTV_ATV);
    sp.setAtvMode(AM_SCAN_ATVMODE_NONE);
    sp.setDtvMode(scan_mode);
    sp.setDtvFrequency1(beginFreq);
    sp.setDtvFrequency2(endFreq);
    return Scan(fe, sp);
}

int CTvScanner::dtvAutoScan(int mode)
{
    return dtvScan(mode, AM_SCAN_DTVMODE_ALLBAND, 0, 0, -1, -1);
}

int CTvScanner::dtvManualScan(int mode, int beginFreq, int endFreq, int para1, int para2)
{
    return dtvScan(mode, AM_SCAN_DTVMODE_MANUAL, beginFreq, endFreq, para1, para2);
}

int CTvScanner::manualDtmbScan(int beginFreq, int endFreq, int modulation)
{
    return dtvManualScan(FE_DTMB, beginFreq, endFreq, modulation, -1);
}


//only test for dtv allbland auto
int CTvScanner::autoDtmbScan()
{
    return dtvAutoScan(FE_DTMB);
}


