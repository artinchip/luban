#ifndef ECSDKSETYPES_H
#define ECSDKSETYPES_H

/* ChiefBeam Index */
#define EC_SE_PARAM_KEY_BEAM_INDEX  					"beam_index"		///< param beamIndex id
#define EC_SE_PARAM_VALUE_BEAM_FRONT_LEFT				"0"					///< param beamIndex value 0, means output the front left channel audio
#define EC_SE_PARAM_VALUE_BEAM_FRONT_RIGTH				"1"					///< param beamIndex value 1, means output the front right channel audio
#define EC_SE_PARAM_VALUE_BEAM_REAR_LEFT				"2"					///< param beamIndex value 2, means output the rear left channel audio
#define EC_SE_PARAM_VALUE_BEAM_REAR_RIGHT				"3"					///< param beamIndex value 4, means output the rear right channel audio
#define EC_SE_PARAM_VALUE_BEAM_REAR_MID				    "4"					///< param beamIndex value 3, means output the rear mid channel audio

/* AEC work mode */
#define EC_SE_PARAM_KEY_AEC_WORK_MODE					"aec_work_mode"		///< param AEC work mode param_id
#define EC_SE_PARAM_VALUE_AEC_OFF_MODE					"1"					///< AEC mode off
#define EC_SE_PARAM_VALUE_AEC_VR_MODE					"2"					///< AEC mode on for VR
#define EC_SE_PARAM_VALUE_AEC_TEL_MODE					"3"					///< AEC mode on for telephone

/* AEC Ref number */
#define EC_SE_PARAM_KEY_AEC_REF_NUMBER					"aec_ref_number"	///< param AEC ref number param_id
#define EC_SE_PARAM_VALUE_AEC_REF_ZERO					"0"					///< no ref
#define EC_SE_PARAM_VALUE_AEC_REF_SGL					"1"					///< single ref
#define EC_SE_PARAM_VALUE_AEC_REF_DBL					"2"					///< double ref
#define EC_SE_PARAM_VALUE_AEC_REF_QUAD					"4"					///< four ref

/* Mic number */
#define EC_SE_PARAM_KEY_MIC_NUMBER						"mic_number"		///< param mic number param_id
#define EC_SE_PARAM_VALUE_MIC_SGL						"1"					///< single mic
#define EC_SE_PARAM_VALUE_MIC_DBL						"2"					///< double mic
#define EC_SE_PARAM_VALUE_MIC_QUAD						"4"					///< four mic

/* SpeechEnhancement work mode */
#define EC_SE_PARAM_KEY_WORK_MODE						"work_mode"			///< param work_mode param_id
#define EC_SE_PARAM_VALUE_VAD_ONLY_MODE				    "0"					///< VAD only mode for XF6010SYE
#define EC_SE_PARAM_VALUE_FAKE_MAE_MODE				    "1"					///< fake MAE mode for XF6010SYE
#define EC_SE_PARAM_VALUE_LSA_MODE						"2"					///< LSA mode
#define EC_SE_PARAM_VALUE_MAE_MODE						"3"					///< MAE mode
#define EC_SE_PARAM_VALUE_MAB_MODE						"4"					///< MAB mode
#define EC_SE_PARAM_VALUE_MAB_AND_MAE_MODE				"5"					///< MAB_AND_MAE mode
#define EC_SE_PARAM_VALUE_FOURVOICE_MAB_MODE			"6"					///< MAB mode for Four Voice
#define EC_SE_PARAM_VALUE_FOURVOICE_MAE_MODE			"7"					///< MAE mode for Four Voice
#define EC_SE_PARAM_VALUE_FOURVOICE_MAB_AND_MAE_MODE	"8"					///< MAB_AND_MAE for Four Voice
#define EC_SE_PARAM_VALUE_BLUETOOTH_MAE_MODE            "9"                 ///< BLUETOOTH_MAE mode
#define EC_SE_PARAM_VALUE_MAB_NN_FOR_6020               "10"                ///< MAB_NN_FOR_6020 mode

#endif