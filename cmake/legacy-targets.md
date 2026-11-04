# Legacy target inventory

This inventory records archive names and the initial source assignments in the legacy Makefiles. Conditional additions, generated sources, dependencies and platform flags still require separate translation. Make variables are retained as written.

| Makefile | Archive | Initial sources |
| --- | --- | --- |
| `jpeg/Makefile` | `libjpeg.a` | CSRCS: jdapimin.c, jdapistd.c, jdatasrc.c, jdmaster.c, jdinput.c, jdmarker.c, jdhuff.c, jdphuff.c, jdmainct.c, jdcoefct.c, jdpostct.c, jddctmgr.c, jidctfst.c, jidctflt.c, jidctint.c, jidctred.c, jdsample.c, jdcolor.c, jquant1.c, jquant2.c, jdmerge.c, jcomapi.c, jutils.c, jerror.c, jmemmgr.c, jmemnobs.c |
| `nspr/src/Makefile` | `nspr.a` | CFILES: file.c, getopt.c, linker.c, log2.c, longlong.c, page.c, prabort.c, prarena.c, prdump.c, prevent.c, prhash.c, prmcache.c, prnetdb.c, prtime.c, prmjtime.c, prfile.c, prfileio.c, prlog.c, prnetio.c, prdtoa.c, prprf.c, prsystem.c, swintr.c, swmon.c, swsched.c, swthread.c |
| `mocha/src/Makefile` | `mocha.a` | SRCS: mo_array.c, mo_atom.c, mo_bcode.c, mo_bool.c, mo_cntxt.c, mo_date.c, mo_emit.c, mo_fun.c, mo_java.c, mo_math.c, mo_num.c, mo_obj.c, mo_parse.c, mo_scan.c, mo_scope.c, mo_str.c, mocha.c, mochaapi.c, mochalib.c |
| `lib/layout/Makefile` | `liblay.a` | CSRCS: bullet.c, clipline.c, layarena.c, laycell.c, laycols.c, laydisp.c, layembed.c, layfind.c, layform.c, layfree.c, laygrid.c, layhrule.c, layimage.c, layinfo.c, laylist.c, laymap.c, layout.c, laysel.c, layspace.c, laysub.c, laytable.c, laytags.c, laytext.c, layutil.c, ptinpoly.c |
| `lib/libdbm/Makefile` | `libdbm.a` | CSRCS: db.c, h_bigkey.c, h_func.c, h_log2.c, h_page.c, hash.c, hash_buf.c, hsearch.c, memmove.c, mktemp.c, ndbm.c, snprintf.c, strerror.c |
| `lib/libi18n/Makefile` | `libi18n.a` | CSRCS: nscstr.c, autocvt.c, b52cns.c, cns2b5.c, cvchcode.c, euc2jis.c, euc2sjis.c, euckr2is.c, fe_ccc.c, is2euckr.c, jis2oth.c, npc.c, sjis2euc.c, sjis2jis.c, testnpc.c, uscan.c, ugen.c, ugendata.c, umap.c, ucs2.c |
| `lib/libimg/Makefile` | `libimg.a` | CSRCS: color.c, colormap.c, dither.c, external.c, gif.c, if.c, ilclient.c, jpeg.c, scale.c, xbm.c |
| `lib/libjar/Makefile` | `—` | See Makefile |
| `lib/libjava/Makefile` | `—` | CSRCS: lj_cons.c, lj_embed.c, lj_init.c, lj_run.c, lj_thred.c |
| `lib/libmime/Makefile` | `libmime.a` | LIB_SRCS: mimei.c, mimecont.c, mimeenc.c, mimeeobj.c, mimeebod.c, mimehdrs.c, mimeiimg.c, mimeleaf.c, mimemalt.c, mimemapl.c, mimemdig.c, mimemmix.c, mimemoz.c, mimempar.c, mimemrel.c, mimemsg.c, mimemult.c, mimeobj.c, mimepbuf.c, mimesun.c, mimetenr.c, mimetext.c, mimethtm.c, mimetpla.c, mimetric.c, mimeunty.c |
| `lib/libmime2/Makefile` | `libmime.a` | See Makefile |
| `lib/libmisc/Makefile` | `libmisc.a` | CSRCS: bkmks.c, bkmutils.c, glhist.c, hotlist.c, mime.c, shist.c, undo.c |
| `lib/libmocha/Makefile` | `libmocha.a` | CSRCS: lm_applt.c, lm_doc.c, lm_embed.c, lm_form.c, lm_hist.c, lm_href.c, lm_img.c, lm_init.c, lm_input.c, lm_msg.c, lm_nav.c, lm_plgin.c, lm_prefs.c, lm_table.c, lm_taint.c, lm_url.c, lm_win.c |
| `lib/libmsg/Makefile` | `libmsg.a` | CSRCS: addr.c, compose.c, composew.c, flag.c, folders.c, mailbox.c, mailsum.c, messages.c, mimehtml.c, msgbiff.c, msginit.c, msgundo.c, msgutils.c, newsgrp.c, newsrc.c, spool.c, threads.c, ad_strm.c, appledbl.c, ap_decod.c, ap_encod.c, bh_strm.c, m_binhex.c |
| `lib/libnet/Makefile` | `libnet.a` | CSRCS: cvactive.c, cvcolor.c, cvdisk.c, cvextcon.c, cvmime.c, cvproxy.c, cvview.c, extcache.c, mkaccess.c, mkautocf.c, mkcache.c, mkconect.c, mkextcac.c, mkfile.c, mkformat.c, mkfsort.c, mkftp.c, mkgeturl.c, mkgopher.c, mkhelp.c, mkhttp.c, mkinit.c, mkmailbx.c, mkmemcac.c, mkmessag.c, mkmocha.c, mknews.c, mknewsgr.c, mkparse.c, mkpop3.c, mkremote.c, mksmtp.c, mksockrw.c, mksort.c, mkstream.c, mkutils.c, txview.c, unix-dns.c |
| `lib/libparse/Makefile` | `libparse.a` | CSRCS: pa_amp.c, pa_hash.c, pa_mdl.c, pa_parse.c |
| `lib/plugin/Makefile` | `libplugin.a` | CSRCS: npassoc.c, npglue.c, upi.c, testupi.c |
| `lib/xlate/Makefile` | `libxlate.a` | CSRCS: font_metrics.c, print.c, ps.c, stubs.c, tblprint.c, text.c, xlateimg.c |
| `lib/xp/Makefile` | `libxp.a` | CSRCS: allxpstr.c, xp_alloc.c, xp_cntxt.c, xp_core.c, xp_error.c, xp_file.c, xp_hash.c, xp_intl.c, xp_list.c, xp_md5.c, xp_reg.c, xp_rgb.c, xp_sec.c, xp_str.c, xp_thrmo.c, xp_time.c, xp_trace.c, xp_wrap.c, xpassert.c, xplocale.c |
| `cmd/xfe/Makefile` | `—` | CSRCS: pref_helpers.c, colors.c, commands.c, convert.c, dialogs.c, dragdrop.c, e_kit.c, fonts.c, forms.c, history.c, hot.c, icons.c, images.c, lay.c, locale.c, mailnews.c, menu.c, movemail.c, mozilla.c, new_manage.c, outline.c, passwd.c, plugin.c, prefdialogs.c, prefs.c, remote-s.c, scroll.c, scroller.c, strids.c, strings.c, thermo.c, visual.c, xfe.c, xfe-dns.c |
| `cmd/xfe/hpane/Makefile` | `libHPane.a` | CSRCS: HPaned.c |
| `cmd/xfe/Microline/XmL/Makefile` | `libXmL.a` | CSRCS: Folder.c, Grid.c, Progress.c, XmL.c |
