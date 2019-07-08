#include <ir/ir.h>
#include <target/util.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static char const* cmake_prefix(void) {
  char const* prefix = getenv("ELVM_CMAKE_PREFIX");
  return prefix ? prefix : "elvm";
}




/* Initializes registers, memory and input
 */
static void cmake_init_data(char const* indent, Data const* data) {

  /* Initialize registers
   */
  for (int i = 0; i < 7; i++) {
    emit_line("%sset_property(GLOBAL PROPERTY \"%s_reg_%s\" \"0\")", indent, cmake_prefix(), reg_names[i]);
  }
  emit_line("");


  /* Initialize memory
   */
  for (int mp = 0; data; data = data->next, mp++) {
    emit_line("%sset_property(GLOBAL PROPERTY \"%s_mem_%d\" \"%d\")", indent, cmake_prefix(), mp, data->v);
  }
  emit_line("");


  /* Initialize GETC source
   */
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_input\" \"\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_input_idx\" \"0\")", indent, cmake_prefix());
  emit_line("");

  emit_line("%sif (EXISTS \"${ELVM_INPUT_FILE}\")", indent);
  emit_line("%s\tfile(READ \"${ELVM_INPUT_FILE}\" \"input\" \"\" HEX)", indent, cmake_prefix());
  emit_line("%s\tstring(TOUPPER \"${input}\" \"input\")", indent);
  emit_line("%s\tset_property(GLOBAL PROPERTY \"%s_input\" \"${input}\")", indent, cmake_prefix());
  emit_line("%sendif()", indent);
  emit_line("");


  /* GETC lookup table
   *
   * @warning Do not convert to loop without considering self hosting! For
   *     example, the elvm libc's implementation of {@link printf} does not
   *     support %02X
   */
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_00\" \"0\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_01\" \"1\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_02\" \"2\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_03\" \"3\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_04\" \"4\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_05\" \"5\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_06\" \"6\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_07\" \"7\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_08\" \"8\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_09\" \"9\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_0A\" \"10\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_0B\" \"11\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_0C\" \"12\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_0D\" \"13\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_0E\" \"14\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_0F\" \"15\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_10\" \"16\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_11\" \"17\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_12\" \"18\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_13\" \"19\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_14\" \"20\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_15\" \"21\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_16\" \"22\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_17\" \"23\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_18\" \"24\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_19\" \"25\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_1A\" \"26\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_1B\" \"27\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_1C\" \"28\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_1D\" \"29\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_1E\" \"30\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_1F\" \"31\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_20\" \"32\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_21\" \"33\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_22\" \"34\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_23\" \"35\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_24\" \"36\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_25\" \"37\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_26\" \"38\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_27\" \"39\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_28\" \"40\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_29\" \"41\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_2A\" \"42\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_2B\" \"43\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_2C\" \"44\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_2D\" \"45\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_2E\" \"46\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_2F\" \"47\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_30\" \"48\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_31\" \"49\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_32\" \"50\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_33\" \"51\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_34\" \"52\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_35\" \"53\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_36\" \"54\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_37\" \"55\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_38\" \"56\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_39\" \"57\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_3A\" \"58\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_3B\" \"59\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_3C\" \"60\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_3D\" \"61\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_3E\" \"62\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_3F\" \"63\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_40\" \"64\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_41\" \"65\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_42\" \"66\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_43\" \"67\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_44\" \"68\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_45\" \"69\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_46\" \"70\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_47\" \"71\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_48\" \"72\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_49\" \"73\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_4A\" \"74\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_4B\" \"75\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_4C\" \"76\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_4D\" \"77\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_4E\" \"78\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_4F\" \"79\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_50\" \"80\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_51\" \"81\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_52\" \"82\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_53\" \"83\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_54\" \"84\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_55\" \"85\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_56\" \"86\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_57\" \"87\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_58\" \"88\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_59\" \"89\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_5A\" \"90\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_5B\" \"91\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_5C\" \"92\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_5D\" \"93\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_5E\" \"94\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_5F\" \"95\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_60\" \"96\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_61\" \"97\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_62\" \"98\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_63\" \"99\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_64\" \"100\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_65\" \"101\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_66\" \"102\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_67\" \"103\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_68\" \"104\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_69\" \"105\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_6A\" \"106\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_6B\" \"107\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_6C\" \"108\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_6D\" \"109\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_6E\" \"110\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_6F\" \"111\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_70\" \"112\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_71\" \"113\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_72\" \"114\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_73\" \"115\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_74\" \"116\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_75\" \"117\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_76\" \"118\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_77\" \"119\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_78\" \"120\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_79\" \"121\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_7A\" \"122\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_7B\" \"123\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_7C\" \"124\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_7D\" \"125\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_7E\" \"126\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_7F\" \"127\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_80\" \"128\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_81\" \"129\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_82\" \"130\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_83\" \"131\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_84\" \"132\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_85\" \"133\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_86\" \"134\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_87\" \"135\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_88\" \"136\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_89\" \"137\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_8A\" \"138\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_8B\" \"139\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_8C\" \"140\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_8D\" \"141\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_8E\" \"142\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_8F\" \"143\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_90\" \"144\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_91\" \"145\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_92\" \"146\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_93\" \"147\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_94\" \"148\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_95\" \"149\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_96\" \"150\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_97\" \"151\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_98\" \"152\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_99\" \"153\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_9A\" \"154\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_9B\" \"155\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_9C\" \"156\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_9D\" \"157\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_9E\" \"158\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_9F\" \"159\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_A0\" \"160\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_A1\" \"161\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_A2\" \"162\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_A3\" \"163\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_A4\" \"164\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_A5\" \"165\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_A6\" \"166\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_A7\" \"167\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_A8\" \"168\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_A9\" \"169\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_AA\" \"170\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_AB\" \"171\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_AC\" \"172\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_AD\" \"173\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_AE\" \"174\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_AF\" \"175\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_B0\" \"176\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_B1\" \"177\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_B2\" \"178\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_B3\" \"179\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_B4\" \"180\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_B5\" \"181\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_B6\" \"182\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_B7\" \"183\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_B8\" \"184\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_B9\" \"185\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_BA\" \"186\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_BB\" \"187\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_BC\" \"188\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_BD\" \"189\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_BE\" \"190\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_BF\" \"191\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_C0\" \"192\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_C1\" \"193\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_C2\" \"194\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_C3\" \"195\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_C4\" \"196\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_C5\" \"197\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_C6\" \"198\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_C7\" \"199\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_C8\" \"200\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_C9\" \"201\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_CA\" \"202\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_CB\" \"203\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_CC\" \"204\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_CD\" \"205\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_CE\" \"206\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_CF\" \"207\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_D0\" \"208\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_D1\" \"209\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_D2\" \"210\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_D3\" \"211\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_D4\" \"212\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_D5\" \"213\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_D6\" \"214\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_D7\" \"215\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_D8\" \"216\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_D9\" \"217\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_DA\" \"218\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_DB\" \"219\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_DC\" \"220\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_DD\" \"221\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_DE\" \"222\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_DF\" \"223\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_E0\" \"224\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_E1\" \"225\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_E2\" \"226\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_E3\" \"227\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_E4\" \"228\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_E5\" \"229\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_E6\" \"230\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_E7\" \"231\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_E8\" \"232\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_E9\" \"233\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_EA\" \"234\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_EB\" \"235\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_EC\" \"236\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_ED\" \"237\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_EE\" \"238\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_EF\" \"239\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_F0\" \"240\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_F1\" \"241\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_F2\" \"242\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_F3\" \"243\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_F4\" \"244\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_F5\" \"245\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_F6\" \"246\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_F7\" \"247\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_F8\" \"248\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_F9\" \"249\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_FA\" \"250\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_FB\" \"251\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_FC\" \"252\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_FD\" \"253\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_FE\" \"254\")", indent, cmake_prefix());
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_getc_FF\" \"255\")", indent, cmake_prefix());
  emit_line("");
}



static void cmake_emit_func_prologue(int func_id) {
  emit_line("# cmake_emit_func_prologue(%d)", func_id);
  emit_line("function(\"%s_chunk_%d\")", cmake_prefix(), func_id);

  /* Function chuck only coveres a part of the total program counter span
   */
  const int min_pc_incl = (func_id + 0) * CHUNKED_FUNC_SIZE;
  const int max_pc_excl = (func_id + 1) * CHUNKED_FUNC_SIZE;

  emit_line("\twhile (TRUE)");
  emit_line("\t\tget_property(\"pc\" GLOBAL PROPERTY \"%s_reg_pc\")", cmake_prefix());
  emit_line("\t\tget_property(\"running\" GLOBAL PROPERTY \"%s_running\")", cmake_prefix());
  emit_line("");
  emit_line("\t\tif (\"${pc}\" LESS \"%d\")", min_pc_incl);
  emit_line("\t\t\tbreak()");
  emit_line("\t\tendif()");
  emit_line("");
  emit_line("\t\tif (\"${pc}\" GREATER_EQUAL \"%d\")", max_pc_excl);
  emit_line("\t\t\tbreak()");
  emit_line("\t\tendif()");
  emit_line("");
  emit_line("\t\tif (\"${running}\" STREQUAL \"FALSE\")");
  emit_line("\t\t\tbreak()");
  emit_line("\t\tendif()");
  emit_line("");

  /* Dummy
   */
  emit_line("\t\t# Dummy before first basic block");
  emit_line("\t\tif (FALSE)");
}



/* Program counter must be increased at end of basic block
 */
static void cmake_emit_end_of_basic_block(void) {
  emit_line("\t\t\tget_property(\"pc\" GLOBAL PROPERTY \"%s_reg_pc\")", cmake_prefix());
  emit_line("\t\t\tmath(EXPR \"pc\" \"${pc} + 1\")");
  emit_line("\t\t\tset_property(GLOBAL PROPERTY \"%s_reg_pc\" \"${pc}\")", cmake_prefix());
  emit_line("\t\t\tcontinue()");
  emit_line("\t\tendif()");
}



static void cmake_emit_func_epilogue(void) {
  emit_line("\t\t\t# cmake_emit_func_epilogue()");
  cmake_emit_end_of_basic_block();
  emit_line("\tendwhile()");
  emit_line("endfunction()");
  emit_line("");
}



static void cmake_emit_pc_change(int pc) {
  emit_line("\t\t\t# cmake_emit_pc_change(%d)", pc);
  cmake_emit_end_of_basic_block();
  emit_line("");
  emit_line("\t\tget_property(\"pc\" GLOBAL PROPERTY \"%s_reg_pc\")", cmake_prefix());
  emit_line("\t\tif (\"${pc}\" EQUAL \"%d\")", pc);
}



static void cmake_emit_debug(char const* indent, char const* message) {
  emit_line("%s# %s", indent, message);
  emit_line("%sif (DEFINED ENV{DEBUG})", indent);
  emit_line("%s\tmessage(STATUS \"%s\")", indent, message);
  emit_line("%sendif()", indent);
  emit_line("");
}



/* Loads `value' into a local variable named `variable'
 *
 * @param indent Indent to use
 * @param variable Local CMake variable name
 * @param value Registry or immediate value
 *
 * @see value_str
 */
static void cmake_emit_read_value(
      char const* indent,
      char const* variable,
      Value const* value
    ) {

  if (REG == value->type) {
    emit_line("%sget_property(\"%s\" GLOBAL PROPERTY \"%s_reg_%s\")", indent, variable, cmake_prefix(), reg_names[value->reg]);

  } else if (IMM == value->type) {
    emit_line("%sset(\"%s\" \"%d\")", indent, variable, value->imm);

  } else {
    error("Invalid value");
  }
}



/* Stores the content of the local variable named `variable' into `value'
 */
static void cmake_emit_write_value(
      char const* indent,
      char const* variable,
      Value const* value
    ) {

  if (REG == value->type) {
    emit_line("%sset_property(GLOBAL PROPERTY \"%s_reg_%s\" \"${%s}\")", indent, cmake_prefix(), reg_names[value->reg], variable);

  } else {
    error("Invalid value");
  }
}



static char const* cmake_op_str(Op op) {
  switch (op) {
  case OP_UNSET:
  case OP_ERR:
  case LAST_OP: {
    error(format("Invalid opcode %d", op));
  } break;

  case MOV: return "MOV";
  case ADD: return "ADD";
  case SUB: return "SUB";
  case LOAD: return "LOAD";
  case STORE: return "STORE";
  case PUTC: return "PUTC";
  case GETC: return "GETC";
  case EXIT: return "EXIT";
  case JEQ: return "JEQ";
  case JNE: return "JNE";
  case JLT: return "JLT";
  case JGT: return "JGT";
  case JLE: return "JLE";
  case JGE: return "JGE";
  case JMP: return "JMP";
  case EQ: return "EQ";
  case NE: return "NE";
  case LT: return "LT";
  case GT: return "GT";
  case LE: return "LE";
  case GE: return "GE";
  case DUMP: return "DUMP";
  }

  error(format("Unsupported opcode %d", op));
}



static char const* cmake_condition(Op op, char const* a, char const* b) {
  switch (op) {
  case JEQ: return format("\"%s\" EQUAL \"%s\"", a, b);
  case JNE: return format("NOT (\"%s\" EQUAL \"%s\")", a, b);
  case JLT: return format("\"%s\" LESS \"%s\"", a, b);
  case JGT: return format("\"%s\" GREATER \"%s\"", a, b);
  case JLE: return format("\"%s\" LESS_EQUAL \"%s\"", a, b);
  case JGE: return format("\"%s\" GREATER_EQUAL \"%s\"", a, b);
  case EQ: return format("\"%s\" EQUAL \"%s\"", a, b);
  case NE: return format("NOT (\"%s\" EQUAL \"%s\")", a, b);
  case LT: return format("\"%s\" LESS \"%s\"", a, b);
  case GT: return format("\"%s\" GREATER \"%s\"", a, b);
  case LE: return format("\"%s\" LESS_EQUAL \"%s\"", a, b);
  case GE: return format("\"%s\" GREATER_EQUAL \"%s\"", a, b);

  default: {/* error */}
  }

  error(format("Opcode %d not supported for comparison", op));
}



static void cmake_emit_jump(char const* indent, char const* target) {

  /* Since the program counter is automatically increased at the end of every
   * basic block, we have to subtract this expected addition from the target
   * address here
   */
  emit_line("%smath(EXPR \"jmp_target\" \"%s - 1\")", indent, target);
  emit_line("%sset_property(GLOBAL PROPERTY \"%s_reg_pc\" \"${jmp_target}\")", indent, cmake_prefix());
}



static void cmake_emit_inst(Inst* inst) {
  emit_line("\t\t\t# cmake_emit_inst(...)");
  switch (inst->op) {


  case MOV: {
    cmake_emit_debug("\t\t\t", format("MOV %s, %s", value_str(&inst->dst), value_str(&inst->src)));

    cmake_emit_read_value("\t\t\t", "mov_tmp", &inst->src);
    cmake_emit_write_value("\t\t\t", "mov_tmp", &inst->dst);
  } break;


  case ADD: {
    cmake_emit_debug("\t\t\t", format("ADD %s, %s", value_str(&inst->dst), value_str(&inst->src)));

    cmake_emit_read_value("\t\t\t", "add_a", &inst->dst);
    cmake_emit_read_value("\t\t\t", "add_b", &inst->src);
    emit_line("\t\t\tmath(EXPR \"add_sum\" \"(${add_a} + ${add_b}) & %s\")", UINT_MAX_STR);
    cmake_emit_write_value("\t\t\t", "add_sum", &inst->dst);
  } break;


  case SUB: {
    cmake_emit_debug("\t\t\t", format("SUB %s, %s", value_str(&inst->dst), value_str(&inst->src)));

    cmake_emit_read_value("\t\t\t", "sub_minuend", &inst->dst);
    cmake_emit_read_value("\t\t\t", "sub_subtrahend", &inst->src);
    emit_line("\t\t\tmath(EXPR \"sub_difference\" \"(${sub_minuend} - ${sub_subtrahend}) & %s\")", UINT_MAX_STR);
    cmake_emit_write_value("\t\t\t", "sub_difference", &inst->dst);
  } break;


  // dst = mem[src]
  case LOAD: {
    cmake_emit_debug("\t\t\t", format("LOAD %s, %s", value_str(&inst->dst), value_str(&inst->src)));

    cmake_emit_read_value("\t\t\t", "load_address", &inst->src);
    emit_line("\t\t\tget_property(\"load_address_set\" GLOBAL PROPERTY \"%s_mem_${load_address}\" SET)", cmake_prefix());
    emit_line("\t\t\tset(\"load_value\" \"0\")");
    emit_line("");

    emit_line("\t\t\tif (\"${load_address_set}\")");
    emit_line("\t\t\t\tget_property(\"load_value\" GLOBAL PROPERTY \"%s_mem_${load_address}\")", cmake_prefix());
    emit_line("\t\t\tendif()");
    emit_line("");

    cmake_emit_write_value("\t\t\t", "load_value", &inst->dst);
  } break;


  // mem[src] = dst;
  case STORE: {
    cmake_emit_debug("\t\t\t", format("STORE %s, %s", value_str(&inst->src), value_str(&inst->dst)));

    cmake_emit_read_value("\t\t\t", "store_address", &inst->src);
    cmake_emit_read_value("\t\t\t", "store_value", &inst->dst);
    emit_line("\t\t\tset_property(GLOBAL PROPERTY \"%s_mem_${store_address}\" \"${store_value}\")", cmake_prefix());
  } break;


  case PUTC: {
    cmake_emit_debug("\t\t\t", format("PUTC %s", value_str(&inst->src)));

    cmake_emit_read_value("\t\t\t", "putc_value_number", &inst->src);

/* string Character with code 0 does not exist.
    emit_line("\t\t\tstring(ASCII \"${putc_value_number}\" \"putc_value_string\")");
    emit_line("\t\t\texecute_process(COMMAND ${CMAKE_COMMAND} -E echo_append \"${putc_value_string}\")");
*/

/* math EXPR called with incorrect arguments.
    emit_line("\t\t\tmath(EXPR \"putc_value_hex\" \"${putc_value_number}\" HEXADECIMAL)");
    emit_line("\t\t\tstring(SUBSTRING \"${putc_value_hex}\" 1 3 \"putc_value_hex\")");
    emit_line("\t\t\texecute_process(COMMAND echo -ne \"\\x${putc_value_hex}\")");
*/

    // Currently using an external helper
    emit_line("\t\t\tif (NOT EXISTS \"${ELVM_PUTC_HELPER}\")");
    emit_line("\t\t\t\tmessage(FATAL_ERROR \"`ELVM_PUTC_HELPER' (${ELVM_PUTC_HELPER}) does not exist\")");
    emit_line("\t\t\tendif()");
    emit_line("\t\t\texecute_process(COMMAND \"${ELVM_PUTC_HELPER}\" \"${putc_value_number}\")");
  } break;


  case GETC: {
    cmake_emit_debug("\t\t\t", format("GETC %s", value_str(&inst->dst)));

    emit_line("\t\t\tget_property(\"getc_input\" GLOBAL PROPERTY \"%s_input\")", cmake_prefix());
    emit_line("\t\t\tget_property(\"getc_input_idx\" GLOBAL PROPERTY \"%s_input_idx\")", cmake_prefix());
    emit_line("\t\t\tstring(LENGTH \"${getc_input}\" \"getc_input_length\")");
    emit_line("");

    /* Read next character from input
     */
    emit_line("\t\t\tif (\"${getc_input_idx}\" LESS \"${getc_input_length}\")");
    emit_line("\t\t\t\tstring(SUBSTRING \"${getc_input}\" \"${getc_input_idx}\" \"2\" \"getc_next_string\")");
    emit_line("\t\t\t\tmath(EXPR \"getc_input_idx\" \"${getc_input_idx} + 2\")");
    emit_line("\t\t\t\tset_property(GLOBAL PROPERTY \"%s_input_idx\" \"${getc_input_idx}\")", cmake_prefix());
    emit_line("");

    /* Use lookup table to convert next byte (in hex) to decimal
     */
    emit_line("\t\t\t\tget_property(\"getc_next_number_set\" GLOBAL PROPERTY \"%s_getc_${getc_next_string}\" SET)", cmake_prefix());
    emit_line("");

    emit_line("\t\t\t\tif (NOT \"${getc_next_number_set}\")");
    emit_line("\t\t\t\t\tmessage(FATAL_ERROR \"Invalid binary input ${getc_next_string}\")");
    emit_line("\t\t\t\tendif()");
    emit_line("");

    emit_line("\t\t\t\tget_property(\"getc_next_number\" GLOBAL PROPERTY \"%s_getc_${getc_next_string}\")", cmake_prefix());
    cmake_emit_write_value("\t\t\t\t", "getc_next_number", &inst->dst);

    /* EOF
     */
    emit_line("\t\t\telse()");
    emit_line("\t\t\t\tset(\"getc_eof\" \"0\")");
    cmake_emit_write_value("\t\t\t\t", "getc_eof", &inst->dst);
    emit_line("\t\t\tendif()");
  } break;


  case EXIT: {
    cmake_emit_debug("\t\t\t", "EXIT");
    emit_line("\t\t\tset_property(GLOBAL PROPERTY \"%s_running\" \"FALSE\")", cmake_prefix());
  } break;


  case JEQ:
  case JNE:
  case JLT:
  case JGT:
  case JLE:
  case JGE: {
    cmake_emit_debug("\t\t\t", format("%s %s, %s, %s", cmake_op_str(inst->op), value_str(&inst->jmp), value_str(&inst->dst), value_str(&inst->src)));

    cmake_emit_read_value("\t\t\t", "jmp_target", &inst->jmp);
    cmake_emit_read_value("\t\t\t", "jmp_compare_a", &inst->dst);
    cmake_emit_read_value("\t\t\t", "jmp_compare_b", &inst->src);
    emit_line("");
    emit_line("\t\t\tif (%s)", cmake_condition(inst->op, "${jmp_compare_a}", "${jmp_compare_b}"));
    cmake_emit_jump("\t\t\t\t", "${jmp_target}");
    emit_line("\t\t\tendif()");
  } break;


  case JMP: {
    cmake_emit_debug("\t\t\t", format("JMP %s", value_str(&inst->jmp)));

    cmake_emit_read_value("\t\t\t", "jmp_target", &inst->jmp);
    cmake_emit_jump("\t\t\t", "${jmp_target}");
  } break;


  case EQ:
  case NE:
  case LT:
  case GT:
  case LE:
  case GE: {
    cmake_emit_debug("\t\t\t", format("%s %s, %s", cmake_op_str(inst->op), value_str(&inst->dst), value_str(&inst->src)));

    cmake_emit_read_value("\t\t\t", "compare_a", &inst->dst);
    cmake_emit_read_value("\t\t\t", "compare_b", &inst->src);
    emit_line("\t\t\tset(\"compare_result\" \"0\")");
    emit_line("");

    emit_line("\t\t\tif (%s)", cmake_condition(inst->op, "${compare_a}", "${compare_b}"));
    emit_line("\t\t\t\tset(\"compare_result\" \"1\")");
    emit_line("\t\t\tendif()");
    emit_line("");

    cmake_emit_write_value("\t\t\t", "compare_result", &inst->dst);
  } break;


  case DUMP: {
    cmake_emit_debug("\t\t\t", "DUMP");
    emit_line("\t\t\t# (no-op)");
  } break;


  default:
    emit_line("\t\t\tmessage(FATAL_ERROR \"Invalid opcode %d\")", inst->op);
    error(format("Invalid opcode %d", inst->op));
    break;
  }

  emit_line("");
}



void target_cmake(Module* module) {
  emit_line("cmake_minimum_required(VERSION %s)", "3.10");
  emit_line("");

  /* Emit definition of all instructions, chunked into groups of basic blocks
   */
  int const num_funcs = emit_chunked_main_loop(
    module->text,
    cmake_emit_func_prologue,
    cmake_emit_func_epilogue,
    cmake_emit_pc_change,
    cmake_emit_inst
  );

  /* Define main function, can be invoked multiple times since memory and
   * register initialization is done on each call
   */
  emit_line("function(\"%s_main\")", cmake_prefix());
  cmake_init_data("\t", module->data);

  emit_line("\twhile (TRUE)");

  /* Check if still running
   */
  emit_line("\t\tget_property(\"running\" GLOBAL PROPERTY \"%s_running\")", cmake_prefix());
  emit_line("");
  emit_line("\t\tif (\"${running}\" STREQUAL \"FALSE\")");
  emit_line("\t\t\tbreak()");
  emit_line("\t\tendif()");
  emit_line("");

  /* Choose correct chunk to execute
   */
  emit_line("\t\tget_property(\"pc\" GLOBAL PROPERTY \"%s_reg_pc\")", cmake_prefix());
  emit_line("\t\tmath(EXPR \"chunk\" \"${pc} / %d\")", CHUNKED_FUNC_SIZE);
  emit_line("");

  for (int func = 0; func < num_funcs; ++func) {
    emit_line("\t\tif (\"%d\" EQUAL \"${chunk}\")", func);
    emit_line("\t\t\t%s_chunk_%d()", cmake_prefix(), func);
    emit_line("\t\t\tcontinue()");
    emit_line("\t\tendif()");
  }
  emit_line("\tendwhile()");
  emit_line("endfunction()");
  emit_line("");

  /* Auto invoke main function
   */
  emit_line("%s_main()", cmake_prefix());
}

