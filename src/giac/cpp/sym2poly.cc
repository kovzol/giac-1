// -*- mode:C++ ; compile-command: "g++-3.4 -DHAVE_CONFIG_H -I. -I..  -DIN_GIAC -g -c sym2poly.cc" -*-
#include "giacPCH.h"
/*
 *  This file implements several functions that work on univariate and
 *  multivariate polynomials and rational functions.
 *  These functions include polynomial quotient and remainder, GCD and LCM
 *  computation, factorization and rational function normalization. */

/*
 *  Copyright (C) 2000,2014 B. Parisse, Institut Fourier, 38402 St Martin d'Heres
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
using namespace std;
#if !defined GIAC_HAS_STO_38 && !defined NSPIRE && !defined FXCG 
#include <fstream>
#endif
#include <string>
#include "gen.h"
#include "sym2poly.h"
#include "usual.h"
#include "unary.h"
#include "subst.h"
#include "modpoly.h"
#include "alg_ext.h"
#include "solve.h"
#include "input_parser.h"
#include "ezgcd.h"
#include "prog.h"
#include "ifactor.h"
#include "poly.h"
#include "plot.h"
#include "misc.h"
#include "modfactor.h"
#include "giacintl.h"
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifndef NO_NAMESPACE_GIAC
namespace giac {
#endif // ndef NO_NAMESPACE_GIAC

  // "instantiate" debugging functions
  void dbgprint(const polynome &p){
    p.dbgprint();
  }

  void dbgprint(const gen & e){
    e.dbgprint();
  }

  vecteur cdr_VECT(const vecteur & l){
    if (l.empty())
      return vecteur(l);
    vecteur::const_iterator it=l.begin(),itend=l.end();
    vecteur res;
    ++it;
    for (;it!=itend;++it)
      res.push_back(*it);
    return vecteur(res);
  }

  //***************************
  // functions relative to lvar
  //***************************
  int equalposcomp(const vecteur & l,const gen & e){
    int n=1;
    for (vecteur::const_iterator it=l.begin();it!=l.end();++it){
      if ((*it)==e)
	return(n);
      else
	n++;
    }
    return(0);
  }

  void addtolvar(const gen & e, vecteur & l){
    if (equalposcomp(l,e))
      return;
    l.push_back(e);
  }

  static void lvar_symbolic(const gen & g, vecteur &l){
    const symbolic & s = *g._SYMBptr;
    if ( (s.sommet==at_plus) || (s.sommet==at_prod)){
      if (s.feuille.type!=_VECT){
	lvar(s.feuille,l);
	return;
      }
      vecteur::iterator it=s.feuille._VECTptr->begin(), itend=s.feuille._VECTptr->end();
      for (;it!=itend;++it)
	lvar(*it,l);
      return;
    }
    if ( (s.sommet==at_neg) || (s.sommet==at_inv) ){
      lvar(s.feuille,l);
      return;
    }
    if ( (s.sommet==at_pow) && ( (*s.feuille._VECTptr)[1].type==_INT_))
      lvar(s.feuille._VECTptr->front(),l);
    else
      addtolvar(g,l);
  }

  void lvar(const vecteur & v,vecteur & l){
    vecteur::const_iterator it=v.begin(),itend=v.end();
    for (;it!=itend;++it)
      lvar(*it,l);
  }

  void lvar(const sparse_poly1 & p,vecteur & l){
    sparse_poly1::const_iterator it=p.begin(),itend=p.end();
    for (;it!=itend;++it){
      lvar(it->coeff,l);
      // lvar(it->exponent,l);
    }
  }

  void lvar(const gen & e, vecteur & l) {
    switch (e.type){
    case _INT_: case _DOUBLE_: case _ZINT: case _CPLX: case _POLY: case _EXT: case _USER: case _REAL:
      return;
    case _IDNT:
      if (strcmp(e._IDNTptr->id_name,string_undef))
	addtolvar(e,l);
      return ;
    case _SYMB:
      lvar_symbolic(e,l);
      return ;
    case _VECT:
      lvar(*e._VECTptr,l);
      return ;
    case _FRAC:
      lvar(e._FRACptr->num,l);
      lvar(e._FRACptr->den,l);
      return;
    case _MOD:
      lvar(*e._MODptr,l);
      lvar(*(e._MODptr+1),l);
      return;
    case _SPOL1:
      lvar(*e._SPOL1ptr,l);
      return;
    default:
      return;
    }
  }
  
  vecteur lvar(const gen & e){
    vecteur l;
    lvar(e,l);
    return l;
  }

  gen symb_lvar(const gen & e){
    return symbolic(at_lvar,e);
  }
  gen cklvar(const gen & e,GIAC_CONTEXT){
    vecteur l;
    lvar(e,l);
    return l;
  }
  static const char _lvar_s []="lvar";
  static define_unary_function_eval (__lvar,&cklvar,_lvar_s);
  define_unary_function_ptr5( at_lvar ,alias_at_lvar,&__lvar,0,true);

  static bool is_algebraic_EXTension(const gen & e){
    if (e.type==_EXT)
      return true;
    if (e.type!=_SYMB)
      return false;
    if ( (e._SYMBptr->sommet==at_sqrt) || (e._SYMBptr->sommet==at_rootof) )
      return true;
    if ( (e._SYMBptr->sommet==at_pow) && (e._SYMBptr->feuille._VECTptr->back().type==_FRAC) && (e._SYMBptr->feuille._VECTptr->back()._FRACptr->den.type==_INT_) &&(absint(e._SYMBptr->feuille._VECTptr->back()._FRACptr->den.val)<=MAX_ALG_EXT_ORDER_SIZE)  )
      return true;
    return false;
  }

  static gen algebraic_argument(const gen & e){
    if (e.type==_EXT)
      return makevecteur(*e._EXTptr,*(e._EXTptr+1));
    if (e.type!=_SYMB)
      return gensizeerr(gettext("sym2poly.cc/algebraic_argument"));
    if ((e._SYMBptr->sommet==at_sqrt) || (e._SYMBptr->sommet==at_rootof) )
      return e._SYMBptr->feuille;
    if ( (e._SYMBptr->sommet==at_pow) && (e._SYMBptr->feuille._VECTptr->back().type==_FRAC) && (e._SYMBptr->feuille._VECTptr->back()._FRACptr->den.type==_INT_) )
      return e._SYMBptr->feuille._VECTptr->front();
    return gentypeerr(gettext("algebraic_argument"));
  }

  static bool equalposmat(const matrice & m,const gen & e,int &i,int & j){
    i=0;
    const_iterateur it=m.begin(),itend=m.end(),jt,jtend;
    for (;it!=itend;++it,++i){
      if (*it==e){
	j=-1;
	return true;
      }
      else {
	if (it->type!=_VECT){
#ifdef NO_STDEXCEPT
	  return false;
#else
	  setsizeerr(gettext("sym2poly.cc/equalposmat"));
#endif
	}
	for (j=0,jt=it->_VECTptr->begin(),jtend=it->_VECTptr->end();jt!=jtend;++jt,++j)
	  if (*jt==e)
	    return true;
      }
    }
    return false;
  }

  static void addfirstrow(const gen & e,matrice & m){
    if (m.empty()){
      vecteur v(1,e);
      m.push_back(v);
    }
    else {
      if (m.front().type!=_VECT){
#ifdef NO_STDEXCEPT
	return;
#else
	setsizeerr(gettext("sym2poly.cc/addfirstrow"));
#endif
      }
      vecteur v(*m.front()._VECTptr);
      v.push_back(e);
      m.front()=v;
    }
  }

  static void remove(matrice & m,const vecteur & v,int l){
    // remove vars in v from m if they are not in line l
    for (int k=0;k<v.size();++k){
      int I,J;
      if (equalposmat(m,v[k],I,J) && I!=l){
        vecteur mI=*m[I]._VECTptr;
        mI.erase(mI.begin()+J);
        m[I]=mI;
      }
    }
  }

  static matrice ext_glue_matrices(const matrice & a,const matrice & b){
    if (a.size()>b.size())
      return ext_glue_matrices(b,a);
#if 0
    // if a variable is common to 2 extensions, merge
    // all variables that are at level >=?
    if (!a.empty()){
      vecteur v(lvar(a));
      lvar(b,v);
      for (int i=0;i<v.size();++i){
        int ia,ja,ib,jb;
        if (equalposmat(a,v[i],ia,ja) && equalposmat(b,v[i],ib,jb)){
          gen aij=a[ia],bij=b[ib];
          if (aij!=bij && aij.type==_VECT && bij.type==_VECT){
            matrice A(a),B(b);
            if (!ia && !ib)
              continue;
            if (ia && ib){
              vecteur common=mergevecteur(*aij._VECTptr,*bij._VECTptr); comprim(common);
              A[ia]=common;
              B[ib]=common;
              remove(A,common,ia);
              remove(B,common,ib);
            }
            else if (ia){
              vecteur B0=*B[0]._VECTptr;
              B0.erase(B0.begin()+jb);
              B[0]=B0;
            }
            else if (ib){
              vecteur A0=*A[0]._VECTptr;
              A0.erase(A0.begin()+ja);
              A[0]=A0;
            }
            return ext_glue_matrices(A,B);
          }
        }
      }
    }
#endif
    // Now a.size()<=b.size()
    // Algorithm should be fixed in all generality FAILS for a=[[],[c,d]] and b=[[b,d],[c]], should return [[b],[c,d]]
    // the loop below fixes assume(a>0); normal(-sqrt(a)*sqrt(a*pi)*sqrt(pi))
    if (a.size()==b.size()){
      for (int i=0;i<a.size();++i){
	if (a[i].type==_VECT && b[i].type==_VECT){
	  if (a[i]._VECTptr->size()<b[i]._VECTptr->size())
	    break;
	  if (a[i]._VECTptr->size()>b[i]._VECTptr->size())
	    return ext_glue_matrices(b,a);
	}
      }
    }
    if (b.empty() || a.empty() || (a==b))
      return b;
    int i,j;
    matrice res(b.begin()+1,b.end()); // all alg. extensions of b
    // look in a for vars that are not inside res
    matrice a_not_in_res;
    const_iterateur it=a.begin(),itend=a.end(),jt,jtend;
    for (;it!=itend;++it){
      vecteur temp;
      for (jt=it->_VECTptr->begin(),jtend=it->_VECTptr->end();jt!=jtend;++jt){
	if (!equalposmat(res,*jt,i,j))
	  temp.push_back(*jt);
      }
      if (it==a.begin() || (!temp.empty()) )
	a_not_in_res.push_back(temp);
    }
    // look in the first row of b for vars that are not inside a_not_in_res
    jt=b.begin()->_VECTptr->begin(),jtend=b.begin()->_VECTptr->end();
    for (;jt!=jtend;++jt){
      if (!equalposmat(a_not_in_res,*jt,i,j))
	addfirstrow(*jt,a_not_in_res);
    }
    return mergevecteur(a_not_in_res,res);
  }

  void alg_lvar(const sparse_poly1 & p,vecteur & l){
    sparse_poly1::const_iterator it=p.begin(),itend=p.end();
    for (;it!=itend;++it){
      alg_lvar(it->coeff,l);
      // lvar(it->exponent,l);
    }
  }

  static vecteur sort1(const vecteur & l);
  // return true if there is at least one algebraic extension
  void alg_lvar(const gen & e,matrice & m){
    vecteur temp;
    lvar(e,temp);
    //temp=sort1(temp);
    int i,j;
    // For each variable of temp, 
    // if not alg var look if still inside m else add it to the first line
    // else make a "merge"
    const_iterateur it=temp.begin(),itend=temp.end();
    for (;it!=itend;++it){
      if ( !is_algebraic_EXTension(*it) ){
	if (!equalposmat(m,*it,i,j)){
	  addfirstrow(*it,m);
	}
      }
      else { // *it is an algebraic extension!
#if 1
	matrice ext_mat;
	vecteur v,vt;
	ext_mat.push_back(v);
	vt=alg_lvar(algebraic_argument(*it));
	int s=int(vt.size());
	if (s>1 || (s==1 && !vt.front()._VECTptr->empty()) ){ 
	    ext_mat=mergevecteur(ext_mat,vt);
	}
	m=ext_glue_matrices(ext_mat,m);
#else
	vecteur vt;
	alg_lvar(algebraic_argument(*it),vt);
	if (vt==m) return;
	matrice ext_mat;
	vecteur v;
	ext_mat.push_back(v);
	int s=int(vt.size());
	if (s>1 || (s==1 && !vt.front()._VECTptr->empty()) )
	  ext_mat=mergevecteur(ext_mat,vt);
	m=ext_glue_matrices(ext_mat,m);
#endif
      }
    }
  }
  gen alg_lvar_f(const gen & g,GIAC_CONTEXT){
    vecteur w=lvar(g);
    return symbolic(at_pow,makesequence(_product(w,contextptr),plus_one_half));
  }
  vecteur alg_lvar(const gen & e){
    vecteur l;
    l.push_back(l); // insure a null line inside the matrix of alg_lvar
    if (has_op(e,*at_rootof)){
      vecteur w0=lvar(e),w;
      for (int i=0;i<w0.size();++i){
	if (w0[i].is_symb_of_sommet(at_rootof))
	  w.push_back(w0[i]);
      }
      vector<const unary_function_ptr *> vu;
      vu.push_back(at_rootof); 
      vector <gen_op_context> vv;
      vv.push_back(alg_lvar_f); // was _nop
      // FIXME: arg of e is two vectors, if rootof is ^, this raises a warning
      gen er=subst(w,vu,vv,false,context0);
      if (er.type==_VECT)
	er=subst(e,w,*er._VECTptr,false,context0);
      else
	er=e;
      alg_lvar(er,l);
    }
    else
      alg_lvar(e,l);
    return l;
  }

  gen symb_algvar(const gen & e){
    return symbolic(at_algvar,e);
  }
  gen ckalgvar(const gen & e,GIAC_CONTEXT){
    vecteur l;
    alg_lvar(e,l);
    return l;
  }
  static const char _algvar_s []="algvar";
  static define_unary_function_eval (__algvar,&ckalgvar,_algvar_s);
  define_unary_function_ptr5( at_algvar ,alias_at_algvar,&__algvar,0,true);

  
  //***********************************************
  // functions relative to fractions for efficiency
  //***********************************************
  /*
  static fraction fpow(const fraction & p,const gen & n){
    if (n.type!=_INT_)
      setsizeerr(gettext("sym2poly.cc/fraction pow"));
    return pow(p,n.val);
  }
  */

  gen ref_polynome2gen(ref_polynome * refptr){
    if (refptr->t.coord.empty()){
      delete refptr;
      return 0;
    }
    if (refptr->t.coord.front().index.is_zero() && is_atomic(refptr->t.coord.front().value) ){
      gen res=refptr->t.coord.front().value;
      delete refptr;
      return res;
    }
    return refptr;
  }

  gen monomial2gen(const monomial<gen> & m){
    if (m.index.is_zero() && is_atomic(m.value) ){
      return m.value;
    }
    return new ref_polynome(m);
  }

  gen simplify3(gen & n,gen & d){
    if (is_one(n) || is_one(d))
      return plus_one;
    if (n.type==_EXT){
      gen n_EXT=*n._EXTptr;
      gen g=simplify(n_EXT,d);
      if (!is_one(g)) n=algebraic_EXTension(n_EXT,*(n._EXTptr+1));
      return g;
    }
    if ((n.type==_POLY) && (d.type==_POLY)){
      ref_polynome * pptr = new ref_polynome(n._POLYptr->dim);
      if (
#ifndef SMARTPTR64
	  n.__POLYptr->ref_count==1 && d.__POLYptr->ref_count==1
#else
	  ((ref_polynome*)(* (ulonglong *) &n >> 16))->ref_count ==1 &&
	  ((ref_polynome*)(* (ulonglong *) &d >> 16))->ref_count ==1	  
#endif
	  ){
	simplify(*n._POLYptr,*d._POLYptr,pptr->t);
	return pptr;
      }
      polynome np(*n._POLYptr),dp(*d._POLYptr);
      simplify(np,dp,pptr->t);
      gen tmpmult(plus_one);
      lcmdeno(pptr->t,tmpmult);
      if (tmpmult.type==_POLY)
	tmpmult=polynome(monomial<gen>(tmpmult,index_t(pptr->t.dim)));
      gen g;
      if (is_one(tmpmult))
	g=pptr;
      else
	g=fraction(tmpmult*pptr,tmpmult); // polynome(tmpmult,np.dim));
      // changed 4 nov. 2012 failure on f(x):=sqrt(x)/(x+1); F:=normal(f'(x));
      // Fix 9/2/2013: np and dp may have denominators if there are _EXT coeffs
      tmpmult=1;
      lcmdeno(np,tmpmult);
      lcmdeno(dp,tmpmult);
      n=np*tmpmult;
      d=dp*tmpmult;
      // moved 18/3/2014 for simplify(acos2atan(acos(sqrt(1-x^2))+acos(x))) 
      if (tmpmult.type==_POLY) tmpmult=polynome(monomial<gen>(tmpmult,index_t(pptr->t.dim)));
      if (is_one(tmpmult))
	return g;
      return fraction(g,tmpmult); // g*tmpmult;
    }
    if (n.type==_POLY) {
      gen l;
      vector< monomial<gen> > :: const_iterator it=n._POLYptr->coord.begin(),itend=n._POLYptr->coord.end();
      if (it<itend-1){
	l=gcd(it->value,(itend-1)->value,context0);
	++it; --itend;
      }
      for (;it!=itend;++it){
	l=gcd(l,it->value,context0);
	if (is_one(l))
	  return l;
      }
      gen g=simplify3(l,d);
      if (is_one(g))
	return g;
      polynome np(*n._POLYptr);
      np=np/g;
      n=np; 
      if (g.type>_DOUBLE_){
	// FIXME embedd g and d inside a polynomial like np was 
	polynome pg(np.dim);
	pg.coord.push_back(monomial<gen>(g,np.dim));
	g=pg;
	polynome pd(np.dim);
	pd.coord.push_back(monomial<gen>(d,np.dim));
	d=pd;
      }
      return g;
    }
    if (d.type==_POLY){
      polynome np(n,d._POLYptr->dim),dp(*d._POLYptr);
      polynome g(np.dim);
      g=simplify(np,dp);
      n=np;
      d=dp;
      return g;
    }
    gen g=gcd(n,d,context0);
    n=n/g; // iquo(n,g);
    d=d/g; // iquo(d,g);
    return g;
  }

  static bool has_EXT(const gen & g){
    if (g.type==_EXT)
      return true;
    if (g.type!=_POLY)
      return false;
    polynome & p = *g._POLYptr;
    vector< monomial<gen> >::const_iterator it=p.coord.begin(),itend=p.coord.end();
    for (;it!=itend;++it){
      if (has_EXT(it->value))
	return true;
    }
    return false;
  }

  static void _FRACadd(const gen & n1, const gen & d1,const gen & n2, const gen & d2, gen & num, gen & den){
    // COUT << n1 << "/" << d1 << "+" << n2 << "/" << d2 << "=";
    if (is_one(d1)){
      num=n1*d2+n2;
      den=d2;
      return;
    }
    if (is_one(d2)){
      num=n2*d1+n1;
      den=d1;
      // COUT << num << "/" << den << "\n";
      return;
    }
    // n1/d1+n2/d2 with g=gcd(d1,d2), d1=d1g*g, d2=d2g*g is
    // (n1*d2g+n2*d1g)/g * 1/(d1g*d2g)
    gen d1g(d1),d2g(d2),coeff(1);
    den=simplify3(d1g,d2g);
    if (den.type==_FRAC){
      coeff=den._FRACptr->den;
      den=den._FRACptr->num;
    }
#if 0
    gen n1g(n1),n2g(n2);
    num=simplify3(n1g,n2g);
    if (num.type==_FRAC){
      den=den*num._FRACptr->den;
      num=num._FRACptr->num;
    }
    n1g=(n1g*d2g+n2g*d1g)*coeff;
    simplify3(n1g,den);
    num=num*n1g;
    den=den*d1g*d2g;
#else
    num=(n1*d2g+n2*d1g)*coeff;
    simplify3(num,den);
    den=den*d1g*d2g;
#endif
  }

  static void _FRACmul(const gen & n1, const gen & d1,const gen & n2, const gen & d2, gen & num, gen & den){
    // COUT << n1 << "/" << d1 << "*" << n2 << "/" << d2 << "=";
    if (is_one(d1)){
      num=n1;
      den=d2;
      simplify3(num,den);
      num=num*n2;
      // COUT << num << "/" << den << "\n";
      return;
    }
    if (is_one(d2)){
      num=n2;
      den=d1;
      simplify3(num,den);
      num=num*n1;
      // COUT << num << "/" << den << "\n";
      return;
    }
    num=n1;
    den=d2;
    simplify3(num,den);
    gen ntemp(n2),dtemp(d1);
    simplify3(ntemp,dtemp);
    num=num*ntemp;
    den=den*dtemp;
    // Further simplifications may occur with _EXT multiplications
    if (has_EXT(ntemp))
      simplify3(num,den);
    // COUT << num << "/" << den << "\n";
  }

  //**********************************
  // symbolic to tensor
  //**********************************
  static bool sym2radd (vecteur::const_iterator debut,vecteur::const_iterator fin,const gen & iext,const vecteur &l,const vecteur & lv, const vecteur & lvnum,const vecteur & lvden, int l_size, gen & num, gen & den,GIAC_CONTEXT,bool sequentially){
    bool totally_converted=true;
    if (sequentially || fin-debut<4){
      gen n1,d1,n2,d2;
      num=zero;
      den=plus_one;
      for (;debut!=fin;++debut){
	totally_converted=totally_converted && sym2r(*debut,iext,l,lv,lvnum,lvden,l_size,n1,d1,contextptr);
	n2=num;
	d2=den;
	_FRACadd(n1,d1,n2,d2,num,den);
      }
    }
    else {
      vecteur::const_iterator milieu=debut+(fin-debut)/2;
      gen n1,d1,n2,d2;
      totally_converted=totally_converted && sym2radd(debut,milieu,iext,l,lv,lvnum,lvden,l_size,n1,d1,contextptr,sequentially);
      totally_converted=totally_converted && sym2radd(milieu,fin,iext,l,lv,lvnum,lvden,l_size,n2,d2,contextptr,sequentially);
      _FRACadd(n1,d1,n2,d2,num,den);
    }
    return totally_converted;
  }

  /*
  static bool sym2rmulold (vecteur::const_iterator debut,vecteur::const_iterator fin,const vecteur &l, const vecteur & lv, const vecteur & lvnum,const vecteur & lvden,int l_size, gen & num, gen & den,GIAC_CONTEXT){
    bool totally_converted=true;
    // First check for a "normal" monomial
    gen coeff=plus_one;
    if (!l.empty()){ 
      bool embedd = l.front().type==_VECT ;
      vecteur l1;
      if (embedd)
	l1=*l.front()._VECTptr;
      else
	l1=l;
      gen tmp;
      int pui,pos;
      index_t i(l_size);
      for (;debut!=fin;++debut){
	if (debut->type<_IDNT)
	  coeff=coeff*(*debut);
	else {
	  if (debut->is_symb_of_sommet(at_pow) && debut->_SYMBptr->feuille._VECTptr->back().type==_INT_){
	    tmp=debut->_SYMBptr->feuille._VECTptr->front();
	    pui=debut->_SYMBptr->feuille._VECTptr->back().val;
	  }
	  else {
	    tmp=*debut;
	    pui=1;
	  }
	  if ( tmp.type==_IDNT && (pos=equalposcomp(l1,tmp)) && pui>=0){
	    i[pos-1] += pui;
	  }
	  else
	    break;
	}
      }
      if (!is_zero(coeff))
	coeff=monomial2gen(monomial<gen>(coeff,i));
    }
    if (fin-debut<4){
      gen n1,d1,n2,d2;
      num=coeff;
      den=plus_one;
      for (;debut!=fin;++debut){
	totally_converted=totally_converted && sym2r(*debut,iext,l,lv,lvnum,lvden,l_size,n1,d1,contextptr);
	n2=num;
	d2=den;
	_FRACmul(n1,d1,n2,d2,num,den);
      }
    }
    else {
      vecteur::const_iterator milieu=debut+(fin-debut)/2;
      gen n1,d1,n2,d2;
      totally_converted=totally_converted && sym2rmulold(debut,milieu,l,lv,lvnum,lvden,l_size,n1,d1,contextptr);
      totally_converted=totally_converted && sym2rmulold(milieu,fin,l,lv,lvnum,lvden,l_size,n2,d2,contextptr);
      _FRACmul(n1,d1,n2,d2,num,den);
      simplify3(coeff,den);
      num=num*coeff;
    }
    return totally_converted;
  }
  */


  static bool sym2rmul(vecteur::const_iterator debut,vecteur::const_iterator fin,const gen & iext,const vecteur &l, const vecteur & lv, const vecteur & lvnum,const vecteur & lvden,int l_size, gen & num, gen & den,GIAC_CONTEXT){
    bool totally_converted=true;
    // First check for a "normal" monomial
    gen coeffnum=plus_one,coeffden=plus_one,coeffn=plus_one,coeffd=plus_one;
    if (!l.empty()){ 
      bool embedd = l.front().type==_VECT ;
      const vecteur & l1 = embedd?*l.front()._VECTptr:l;
      gen tmp;
      int pui,pos;
      index_t inum(l_size),iden(l_size);
      for (;debut!=fin;++debut){
	if (debut->type<_IDNT)
	  coeffn=coeffn*(*debut);
	else {
	  if (debut->is_symb_of_sommet(at_inv)){
	    tmp=debut->_SYMBptr->feuille;
	    if (tmp.type<_IDNT){
	      coeffd=coeffd*tmp;
	      continue;
	    }
	    pui=-1;
	  }
	  else {
	    if (debut->is_symb_of_sommet(at_pow) && debut->_SYMBptr->feuille._VECTptr->back().type==_INT_){
	      tmp=debut->_SYMBptr->feuille._VECTptr->front();
	      pui=debut->_SYMBptr->feuille._VECTptr->back().val;
	    }
	    else {
	      tmp=*debut;
	      pui=1;
	    }
	  }
	  if (absint(pui)>=(1<<15))
	    coeffn=gensizeerr(gettext("Polynomial exponent overflow."));
	  if ( (tmp.type==_IDNT || tmp.type==_SYMB) && (pos=equalposcomp(l1,tmp)) ){
	    if (pui>=0) 
	      inum[pos-1] += pui;
	    else
	      iden[pos-1] -= pui;
	  }
	  else
	    break;
	}
      }
      if (!is_exactly_zero(coeffn)){
	// simplify inum and iden
	bool hasnum=false,hasden=false;
	for (int i=0;i<l_size;i++){
	  if (inum[i]<iden[i]){
	    iden[i] -= inum[i];
	    inum[i] = 0;
	    hasden=true;
	  }
	  else {
	    inum[i] -= iden[i];
	    iden[i] = 0;
	    hasnum=true;
	  }
	}
	simplify3(coeffn,coeffd);
	if (hasnum || coeffn.type==_POLY)
	  coeffnum=monomial2gen(monomial<gen>(coeffn,inum));
	else
	  coeffnum=coeffn;
	if (hasden || coeffd.type==_POLY)
	  coeffden=monomial2gen(monomial<gen>(coeffd,iden));
	else
	  coeffden=coeffd;
      }
      else
	coeffnum=0;
    }
    if (iext!=0){
      gen numr,numi;
      reim(coeffden,numr,numi,contextptr);
      if (!is_zero(numi)){
	coeffnum=coeffnum*(numr-cst_i*numi);
	coeffden=numr*numr+numi*numi;
      }
      reim(coeffnum,numr,numi,contextptr);
      if (!is_zero(numi)){
	coeffnum=numr+iext*numi;
	fxnd(coeffnum,numr,numi);
	coeffnum=numr;
	coeffden=coeffden*numi;
      }
    }
    if (fin-debut<4){
      num=coeffnum;
      den=coeffden;
      gen n1,d1,n2,d2;
      for (;debut!=fin;++debut){
	totally_converted=totally_converted && sym2r(*debut,iext,l,lv,lvnum,lvden,l_size,n1,d1,contextptr);
	n2=num;
	d2=den;
	_FRACmul(n1,d1,n2,d2,num,den);
      }
    }
    else {
      vecteur::const_iterator milieu=debut+(fin-debut)/2;
      gen n1,d1,n2,d2,n3,d3;
      totally_converted=totally_converted && sym2rmul(debut,milieu,iext,l,lv,lvnum,lvden,l_size,n1,d1,contextptr);
      totally_converted=totally_converted && sym2rmul(milieu,fin,iext,l,lv,lvnum,lvden,l_size,n2,d2,contextptr);
      _FRACmul(n1,d1,n2,d2,n3,d3);
      _FRACmul(coeffnum,coeffden,n3,d3,num,den);
    }
    return totally_converted;
  }

  static bool sym2rxrootnum(vecteur & v,const vecteur & lv,gen & num,gen & tmpden,GIAC_CONTEXT){
    // make the polynomial X^d - num
    // check for irreducibility
    polynome p(poly12polynome(v));
    polynome p_content(p.dim);
    factorization f;
    gen extra_div=1;
    if (!factor(p,p_content,f,true,false,false,1,extra_div)){
#ifndef NO_STDEXCEPT
      setsizeerr(gettext("Can't check irreducibility extracting rootof"));
#endif
      return false;
    }
    // now we choose the factor of lowest degree of the factorization
    int lowest_degree=int(v.size()),deg;
    factorization::const_iterator f_it,f_itend=f.end();
    // Rewrite num if it's an ext with i, because it is rewritten by factor
    if (num.type==_EXT && has_i(num)){ 
      gen b=1;
      for (f_it=f.begin();f_it!=f_itend;++f_it){
	b=b*f_it->fact.coord.back().value;
      }
      if (b.type==_EXT){
	gen q=evalf(b,1,contextptr)/evalf(num,1,contextptr);
	gen qr=_round(q,contextptr);
	if (is_zero(q-qr,contextptr)){
	  num=b/qr;
	  if (num.type==_FRAC){
	    // commented since x^d=num is solved using irr_p, translated to b below
	    // tmpden=tmpden*num._FRACptr->den; 
	    num=num._FRACptr->num;
	  }
	}
      }
    }
    gen prev=1;
    gen numprev=r2sym(rdiv(-v.back(),v.front(),contextptr),lv,contextptr),numprevf;
    bool hasevalf=has_evalf(numprev,numprevf,1,contextptr);
    if (hasevalf)
      numprevf=arg(numprevf,contextptr);
    for (f_it=f.begin();f_it!=f_itend;++f_it){
      polynome irr_p(f_it->fact);
      deg=irr_p.lexsorted_degree();
      if (!deg)
	continue;
      if (deg==1){
	vecteur vcur=polynome2poly1(irr_p);
	lowest_degree=1;
	gen numcur=rdiv(-vcur.back(),vcur.front(),contextptr);
	// cerr << "xroot" << num << "\n";
	gen numlv=r2sym(numcur,lv,contextptr);
	// select root by computing argument
	if (!hasevalf){
	  *logptr(contextptr) << gettext("Warning, checking for positivity of a root depending of parameters might return wrong sign: ")<< numlv << "\n";
	  v=vcur;
	  num=numcur;
	  break;
	}
	gen numlvf=evalf(numlv,1,contextptr);
	if (is_positive(numlvf,contextptr)){
	  v=vcur;
	  num=numcur;
	  break;
	}
	int d=v.size()-1;
	gen cur0=arg(numlvf,contextptr)-numprevf/gen(d),cur=d*cur0;
        gen N=_round(cur/cst_two_pi,contextptr);
        cur=cur-N*cst_two_pi;
        cur=abs(cur,contextptr);
	if (is_greater(prev,cur,contextptr)){
          bool doit=!is_zero(N) && v.front()==1; //  && v.back()==-num; // extensions are not represented the same...
          for (int i=1;doit && i<d;++i){
            if (!is_zero(v[i]))
              doit=false;
          }
	  v=vcur;
          num=numcur;
          // fix argument, only valid if v==x^d-num
          if (doit){
            gen f=exp(-N*cst_two_pi*cst_i/gen(d),contextptr);
            f=sym2r(f,vecteur(1,vecteur(0)),contextptr);
            gen n,d;
            fxnd(f,n,d);
            if (n.type==_POLY){
              n=n._POLYptr->coord.front().value; // FIXME?? rewrite i in n using extensions of numcur??
              gen fixarg=n/d;
              num=numcur*fixarg;
              v.back()=v.back()*fixarg;
            }
          }
	  prev=cur;
	}
      }
      if (deg==2 && deg==lowest_degree){
	vecteur tmp=polynome2poly1(irr_p);
	gen delta=tmp[1]*tmp[1]-4*tmp[0]*tmp[2];
	delta=r2sym(delta,lv,contextptr);
	if (is_positive(delta,contextptr))
	  v=tmp;
      }
      if (deg>=lowest_degree)
	continue;
      v=polynome2poly1(irr_p);
      lowest_degree=deg;
    }
    if (lowest_degree>1){
      // here we must check that num is not an extension!!
      if (num.type==_EXT){
	gen a=*(num._EXTptr+1),b;
	gen a__VECTg;
	if (a.type==_VECT)
	  a__VECTg=a;
	else {
	  if( a.type!=_EXT || (a._EXTptr+1)->type!=_VECT){
#ifndef NO_STDEXCEPT
	    setsizeerr(gettext("sym2poly.cc/sym2rxroot"));
#endif
	    return false;
	  }
	  a__VECTg=*(a._EXTptr+1);
	}
	int k;
	vecteur lvadd(lv); lvadd.insert(lvadd.begin(),vecteur(0));
	gen new_v=common_minimal_POLY(a__VECTg,v,a,b,k,
				      // 0.
				      &lvadd ,
				      contextptr);
	if (is_undef(new_v))
	  return false;
	*(num._EXTptr+1)=a;
	if (b.type==_FRAC){
	  num=b._FRACptr->num;
	  tmpden=tmpden*b._FRACptr->den;
	}
	else
	  num=b;
      }
      else {
	vecteur w(2);
	w[0]=plus_one;
	num=algebraic_EXTension(w,v);
      }
    }
    return true;
  }

  void fxnd(const gen & e,gen & num, gen & den){
    if (e.type==_FRAC){
      num=e._FRACptr->num;
      den=e._FRACptr->den;
    }
    else {
      num=e;
      den=plus_one;
    }
  }

  static factorization rsqff(const polynome & p){
    polynome s(lgcd(p));
    factorization f(sqff(p/s));
    if (p.dim==0){
      f.push_back(facteur<polynome>(p,1));
      return f;
    }
    if (p.dim==1){
      // adjust const coeff
      gen p1=1;
      factorization::iterator it=f.begin(),itend=f.end();
      for (;it!=itend;++it){
	p1 = p1*pow(it->fact.coord.front().value,it->mult);
      }
      p1=p.coord.front().value/p1;
      if (is_positive(-p1,context0)){ // ok
	for (it=f.begin();it!=itend;++it){
	  if (it->mult%2){
	    it->fact=-it->fact;
	    p1=-p1;
	    break;
	  }
	}
      }
      if (!is_one(p1)){
        if (p1.type==_POLY){
          factorization fp1=rsqff(*p1._POLYptr);
          for (int i=0;i<fp1.size();++i)
            f.push_back(facteur<polynome>(polynome(fp1[i].fact,1),fp1[i].mult));
        }
        else
          f.push_back(facteur<polynome>(polynome(p1,1),1));
      }
      return f;
    }
    factorization ff(rsqff(s.trunc1()));
    factorization::const_iterator it=ff.begin(),itend=ff.end();
    for (;it!=itend;++it)
      f.push_back(facteur<polynome>(it->fact.untrunc1(),it->mult));
    return f;
  }

  void smaller_factor(vecteur & v){
    // factor v, select smaller factor
    polynome vp(1),vp_content;
    vp=poly12polynome(v,1);
    gen tmp(1); lcmdeno(vp,tmp); vp=tmp*vp;
    factorization vf;
    gen extra_div=1;
    if (factor(vp,vp_content,vf,true,false,false,1,extra_div) && vf.size()>1){
      polynome2poly1(vf.front().fact,1,v);
      for (unsigned i=1;i<vf.size();++i){
	vecteur vi;
	polynome2poly1(vf[i].fact,1,vi);
	if (vi.size()<v.size())
	  v=vi;
      }
    }
  }
  
  static bool sym2rxroot(gen & num,gen & den,int n,int d,const vecteur & l,GIAC_CONTEXT){
    if (is_zero(num))
      return true;
    if (den.type==_CPLX){
      gen denr,deni,numr,numi;
      reim(den,denr,deni,contextptr);
      den=denr*denr+deni*deni;
      num=num*(denr-cst_i*deni);
      reim(num,numr,numi,contextptr);
      deni=gcd(gcd(numr,denr,contextptr),numi,contextptr);
      num=num/deni;
      den=den/deni;
    }
    if (is_positive(r2e(-den,l,contextptr),contextptr)){
      num=-num;
      den=-den;
    }
    bool sign_changed=false;
    if (d<0){
      n=-n;
      d=-d;
    }
    if (n<0){
      if (num.type==_EXT){
	gen temp(inv_EXT(num)*den);
	fxnd(temp,num,den);
      }
      else {
#if defined RTOS_THREADX || defined BESTA_OS || defined USTL
	gen g=num; num=den; den=g;
#else
	swap(num,den);
#endif
      }
      num=pow(num,-n)*pow(den,n*(1-d));
      den=pow(den,-n);
    }
    else {
      num=pow(num,n)*pow(den,n*(d-1));
      den=pow(den,n);
    }
    /* d==2 test makes normal(sqrt(1-x)) -> i*sqrt(x-1) 
       commented!, should common_minimal_poly detect [1,0...0,+/-same_poly]?
    if ( (d%2 || d=2) && ( 
			   (num.type==_POLY && is_positive(evalf_double(-num._POLYptr->coord.front().value,2,0),0))
		    ) ){
      num=-num;
      sign_changed=true;
    }
    */
    // compute number of cst polynomial in num and keep track of dims
    int embeddings=0;
    vector<int> embeddings_s;
    if (is_atomic(num)){
      const_iterateur it=l.begin(),itend=l.end();
      if (it!=itend && it->type!=_VECT){
        embeddings=1;
        embeddings_s.push_back(itend-it);
      }
      else {
        embeddings=int(itend-it);
        if (
            0 &&  // disable for expressions with mixed rootof/fracpow?
            embeddings==1 && it->type==_VECT && it->_VECTptr->empty())
          embeddings=0; 
        else {
          for (int j=0;j<embeddings;++it,++j){ 
            if (it->type!=_VECT){
              string s("sym2rxroot error num="+num.print(contextptr)+" den="+den.print(contextptr)+" l="+gen(l).print(contextptr));
              CERR << s << "\n";
#ifndef NO_STDEXCEPT
              setsizeerr(s);
#endif
              return false;
            }
            embeddings_s.push_back(int(it->_VECTptr->size()));
          }
        }
      }
    }
    else {
      for (;((num.type==_POLY) && (Tis_constant<gen>(*num._POLYptr)));++embeddings){
	embeddings_s.push_back(num._POLYptr->dim);
	num=num._POLYptr->coord.front().value;
      }
    }
    while (num.type==_FRAC){
      den=den*num._FRACptr->den;
      num=num._FRACptr->num*pow(num._FRACptr->den,d-1);
    }
    vecteur v(d+1,zero);
    gen tmpden=1; 
    if (num.type==_POLY){
      const factorization f=rsqff(*num._POLYptr);
      polynome S(plus_one,num._POLYptr->dim),D(plus_one,S.dim);
      factorization::const_iterator jt=f.begin(),jtend=f.end();
      for (;jt!=jtend;++jt){
	if (jt->mult%d)
	  S=S*pow(jt->fact,jt->mult % d);
	D=D*pow(jt->fact,jt->mult/d);
      }
      // Check sign of D
      vecteur Dl(l);
      if (embeddings && Dl[embeddings].type==_VECT){
        Dl=vecteur(l.begin()+embeddings,l.end());
	// Dl=*Dl[embeddings]._VECTptr;
      }
      if (is_positive(r2e(-D,Dl,contextptr),contextptr)){
	D=-D;
	if (d%2)
	  S=-S;
      }
      gen cont=ppz(S);
      gen simpl,doubl; bool pos;
      zint2simpldoublpos(cont,simpl,doubl,pos,d,contextptr);
      if (!pos) simpl=-simpl;
      num=simpl*S;
      D=doubl*D;
      v.front()=plus_one;
      v.back()=-num;
      sym2rxrootnum(v,vecteur(l.begin()+embeddings,l.end()),num,tmpden,contextptr);
      if (num.type==_EXT && num._EXTptr->type==_VECT){
	num=algebraic_EXTension(multvecteur(D,*(*num._EXTptr)._VECTptr),*(num._EXTptr+1));
      }
      else {
	if (num.type==_POLY)
	  num=D**num._POLYptr;
	else
	  num=D*num;
      }
    }
    else {
      v.front()=plus_one;
      v.back()=-num;
      if (is_integer(num)){
	gen simpl,doubl;
	bool pos;
	zint2simpldoublpos(num,simpl,doubl,pos,d,contextptr);
	v.back()=pos?-simpl:simpl;
	smaller_factor(v);
	if (is_minus_one(v.back()))
	  num=doubl;
	else
	  num=algebraic_EXTension(makevecteur(doubl,0),v);
      }
      else {
	bool neg=false; 
#if 1
	gen numd;
	if (has_evalf(num,numd,1,contextptr) && is_positive(-numd,contextptr))
	  neg=true;
	if (neg){
	  num=-num;
	  v.back()=-num;
	}
#endif
	sym2rxrootnum(v,vecteur(l.begin()+embeddings,l.end()),num,tmpden,contextptr);
	if (neg)
	  num=cst_i*num;
      }
    }
    // end else num integer
    // and eventually we embedd num 
    for (;embeddings;--embeddings){
      num=polynome(num,embeddings_s.back());
      tmpden=polynome(tmpden,embeddings_s.back());
      embeddings_s.pop_back();
    }
    if (sign_changed){
      if (d==2)
	num=cst_i*num;
      else
	num=-num;
    }
    den=den*tmpden;
    return true;
  }

  static bool sym2r (const symbolic &s,const gen & iext,const vecteur &l, const vecteur & lv, const vecteur & lvnum,const vecteur & lvden, int l_size,gen & num,gen & den,GIAC_CONTEXT){
    if (s.sommet==at_plus){
      if (s.feuille.type!=_VECT){
	return sym2r(s.feuille,iext,l,lv,lvnum,lvden,l_size,num,den,contextptr);
      }
      vecteur::iterator debut=s.feuille._VECTptr->begin();
      vecteur::iterator fin=s.feuille._VECTptr->end();
      bool sequentially=has_op(s.feuille,*at_inv) && (fin-debut<512);
      return sym2radd(debut,fin,iext,l,lv,lvnum,lvden,l_size,num,den,contextptr,sequentially);
    }
    if (s.sommet==at_prod){
      vecteur::iterator debut=s.feuille._VECTptr->begin();
      vecteur::iterator fin=s.feuille._VECTptr->end();
      bool res=sym2rmul(debut,fin,iext,l,lv,lvnum,lvden,l_size,num,den,contextptr);
      /*
      gen numtmp,dentmp;
      sym2rmulold(debut,fin,l,lv,lvnum,lvden,l_size,numtmp,dentmp,contextptr);
      if (numtmp!=num || dentmp!=den)
	return false;
      */
      return res;
    }
    if (s.sommet==at_neg){
      bool totally_converted=sym2r(s.feuille,iext,l,lv,lvnum,lvden,l_size,num,den,contextptr);
      num=-num;
      return totally_converted;
    }
    if (s.sommet==at_inv){
      bool totally_converted=sym2r(s.feuille,iext,l,lv,lvnum,lvden,l_size,num,den,contextptr);
      if (is_zero(num)){
	num=unsigned_inf;
	den=plus_one;
	return true;
      }
      if (num.type==_EXT){
	gen temp(inv_EXT(num)*den);
	fxnd(temp,num,den);
      }
      else {
	if ( (num.type==_POLY) && (num._POLYptr->dim==0) && (!num._POLYptr->coord.empty()) && (num._POLYptr->coord.front().value.type==_EXT) ){
	  gen temp(inv_EXT(num._POLYptr->coord.front().value));
	  if ( (den.type==_POLY) && (den._POLYptr->dim==0) && (!den._POLYptr->coord.empty()) )
	    temp=temp*den._POLYptr->coord.front().value;
	  else
	    temp=temp*den;
	  gen tempnum,tempden;
	  fxnd(temp,tempnum,tempden);
	  polynome tmpnum(0),tmpden(0);
	  tmpnum.coord.push_back(monomial<gen>(tempnum,index_t(0)));
	  tmpden.coord.push_back(monomial<gen>(tempden,index_t(0)));
	  num=tmpnum;
	  if (tempden.type==_POLY)
	    den=tmpden;
	  else
	    den=tempden;
	}
	else {
#if defined RTOS_THREADX || defined BESTA_OS || defined USTL
	  gen g=num; num=den; den=g;
#else
	  swap(num,den);
#endif
	}
      }
      return totally_converted;
    }
    if (s.sommet==at_pow){
      if ((*s.feuille._VECTptr)[1].type==_INT_) {
	bool totally_converted=sym2r(s.feuille._VECTptr->front(),iext,l,lv,lvnum,lvden,l_size,num,den,contextptr);
	int n=(*s.feuille._VECTptr)[1].val;
	if (absint(n)>=(1<<15))
	  num=gensizeerr(gettext("Polynomial exponent overflow."));
	if (n<0){
	  if (num.type==_EXT){
	    gen temp(inv_EXT(num)*den);
	    fxnd(temp,num,den);
	  }
	  else {
#if defined RTOS_THREADX || defined BESTA_OS || defined USTL
	    gen g=num; num=den; den=g;
#else
	    swap(num,den);
#endif
	  }
	  num=pow(num,-n);
	  den=pow(den,-n);
	}
	else {
	  num=pow(num,n);
	  den=pow(den,n);
	}
	if (num.type==_EXT)
	  simplify3(num,den);
	// FIXME: embeddings like for rootof
	return totally_converted;
      }
      if ((*s.feuille._VECTptr)[1].type==_FRAC) {
	fraction f=*((*s.feuille._VECTptr)[1]._FRACptr);
	if ( (f.num.type==_INT_) && (f.den.type==_INT_)){
	  bool totally_converted=sym2r(s.feuille._VECTptr->front(),iext,l,lv,lvnum,lvden,l_size,num,den,contextptr);
	  if (!sym2rxroot(num,den,f.num.val,f.den.val,l,contextptr))
	    return false;
	  return totally_converted;
	}
      }
    }
    if (s.sommet==at_rootof){
      bool totally_converted=sym2r(s.feuille._VECTptr->front(),iext,l,lv,lvnum,lvden,l_size,num,den,contextptr);
      gen pmin_num,pmin_den;
      totally_converted=totally_converted && sym2r(s.feuille._VECTptr->back(),iext,l,lv,lvnum,lvden,l_size,pmin_num,pmin_den,contextptr);
      if (!is_one(pmin_den)){
#ifndef NO_STDEXCEPT
	setsizeerr(gettext("Minimal poly. in rootof must be fraction free"));
#endif
	return false;
      }
      int embeddings=0;
      vector<int> embeddings_s;
      // put out constant polynomials
      if (num.type!=_VECT || pmin_num.type!=_VECT)
	return totally_converted;
      vecteur vnum=*num._VECTptr,vpmin=*pmin_num._VECTptr;
      int s=int(l.size());
      for (;embeddings<s;){
	bool exitmainloop=false;
	iterateur it=vnum.begin(),itend=vnum.end();
	int i=0;
	for (;;++it){
	  if (it==itend){
	    if (i==0){
	      ++i;
	      it=vpmin.begin();
	      itend=vpmin.end();
	    }
	    else
	      break;
	  }
	  if (is_atomic(*it))
	    continue;
	  if (it->type==_POLY && Tis_constant<gen>(*it->_POLYptr))
	    continue;
	  exitmainloop=true;
	  break;
	}
	if (exitmainloop)
	  break;
	if (l[embeddings].type!=_VECT){
#ifndef NO_STDEXCEPT
	  setsizeerr(gettext("sym2poly.cc/bool sym2r (const"));
#endif
	  return false;
	}
	embeddings_s.push_back(int(l[embeddings]._VECTptr->size()));
	++embeddings;
	for (it=vnum.begin(),itend=vnum.end(),i=0;;++it){
	  if (it==itend){
	    if (i==0){
	      ++i;
	      it=vpmin.begin();
	      itend=vpmin.end();
	    }
	    else
	      break;
	  }
	  if (it->type==_POLY)
	    *it=it->_POLYptr->coord.front().value;
	}
      }
      num=algebraic_EXTension(vnum,vpmin);
      for (;embeddings;--embeddings){
	num=polynome(num,embeddings_s.back());
	embeddings_s.pop_back();
      }
      return totally_converted;
    }
    num=s;
    den=plus_one;
    return false;
  }

  static bool sym2r (const fraction &f,const gen &iext,const vecteur &l, const vecteur & lv, const vecteur & lvnum,const vecteur & lvden, int l_size,gen & num,gen & den,GIAC_CONTEXT){
    gen dent,numt;
    bool totally_converted=sym2r(f.num,iext,l,lv,lvnum,lvden,l_size,num,dent,contextptr);
    totally_converted=totally_converted && sym2r(f.den,iext,l,lv,lvnum,lvden,l_size,den,numt,contextptr);
    num=num*numt;
    den=den*dent;
    return totally_converted;
  }

  bool sym2r(const vecteur &v,const gen & iext,const vecteur &l, const vecteur & lv, const vecteur & lvnum,const vecteur & lvden, int l_size,gen & num,gen & den,GIAC_CONTEXT){
    den=plus_one;
    if (v.empty()){
      num=zero;
      return true;
    }
    bool totally_converted=true;
    gen lcmdeno=plus_one;
    const_iterateur it=v.begin(),itend=v.end();
    vecteur res,numv;
    res.reserve(2*(itend-it));
    numv.reserve(itend-it);
    for (;it!=itend;++it){
      totally_converted=totally_converted && sym2r(*it,iext,l,lv,lvnum,lvden,l_size,num,den,contextptr);
      lcmdeno = lcm(lcmdeno,den);
      res.push_back(num);
      res.push_back(den);
    }
    for (it=res.begin(),itend=res.end();it!=itend;){
      num=*it;
      ++it;
      den=*it;
      ++it;
      numv.push_back(num*rdiv(lcmdeno,den,contextptr));
    }
    den=lcmdeno;
    num=numv;
    return totally_converted;
  }

  bool sym2r(const vecteur &v,const vecteur &l, const vecteur & lv, const vecteur & lvnum,const vecteur & lvden, int l_size,gen & num,gen & den,GIAC_CONTEXT){
    return sym2r(v,1,l,lv,lvnum,lvden,l_size,num,den,contextptr);
  }

  static bool sym2rmod (const gen * gptr,const gen & iext,const vecteur &l, const vecteur & lv, const vecteur & lvnum,const vecteur & lvden, int l_size,gen & num,gen & den,GIAC_CONTEXT){
    gen modnum,modden,modulo;
    bool totally_converted=sym2r(*(gptr+1),iext,l,lv,lvnum,lvden,l_size,modnum,modden,contextptr);
    modulo=rdiv(modnum,modden,contextptr);
    totally_converted=totally_converted && sym2r(*gptr,iext,l,lv,lvnum,lvden,l_size,num,den,contextptr);
    num=makemod(num,modulo);
    den=makemod(den,modulo);
    return totally_converted;
  }

  // iext is an additional parameter (added 5 sept 2014)
  // value is 0 if i=sqrt(-1) is not in the algebraic number field from lvnum
  // otherwise it is an extension or fraction extension/integer.
  bool sym2r (const gen &e,const gen & iext,const vecteur &l, const vecteur & lv, const vecteur & lvnum,const vecteur & lvden, int l_size,gen & num,gen & den,GIAC_CONTEXT){
    int n=0;
    switch (e.type ) {
    case _INT_: case _DOUBLE_: case _ZINT: case _REAL: case _FLOAT_:
      num=e;
      den=plus_one;
      return true; 
    case _CPLX: 
      if (iext==0){
	num=e;
	den=plus_one;
	return true; 
      }
      if (iext.type!=_FRAC){
	num=*e._CPLXptr+iext*(*(e._CPLXptr+1));
	den=plus_one;
	return true;
      }
      num=*(e._CPLXptr+1);
      den=iext._FRACptr->den;
      simplify3(num,den);
      num=*e._CPLXptr*den+iext._FRACptr->num*num;
      return true;
    case _IDNT: case _SYMB:
      if (e.is_symb_of_sommet(at_rootof) && e._SYMBptr->feuille.type==_VECT && e._SYMBptr->feuille._VECTptr->size()==2){
	gen r=e._SYMBptr->feuille._VECTptr->back();
	for (int i=0;i<lv.size() && i<lvnum.size();++i){
	  if (lv[i].is_symb_of_sommet(at_rootof) && lv[i]._SYMBptr->feuille.type==_VECT && lv[i]._SYMBptr->feuille._VECTptr->size()==2){
	    gen gl=lv[i]._SYMBptr->feuille._VECTptr->front();
	    if (gl.type==_VECT && gl._VECTptr->size()==2 && gl._VECTptr->front()==1 && gl._VECTptr->back()==0){
	      gl=lv[i]._SYMBptr->feuille._VECTptr->back();
	      if (r.type==_VECT && gl.type==_VECT && *gl._VECTptr==*r._VECTptr 
		  //&& lidnt(gl).empty()
		  ){
		n=i+1; // break;
		r=e._SYMBptr->feuille._VECTptr->front();
		sym2r(r,iext,l,lv,lvnum,lvden,l_size,num,den,contextptr);
		r=num;
		if (is_one(den) && r.type==_VECT){
		  gen N=lvnum[n-1];
		  gen D=lvden[n-1];
		  hornerfrac(*r._VECTptr,N,D,num,den);
		  return true;
		}
	      }
	    }
	  }
	}
	if (n && n<=lvnum.size() && e._SYMBptr->feuille._VECTptr->front().type==_VECT){
	  gen N=lvnum[n-1];
	  gen D=lvden[n-1];
	  hornerfrac(*e._SYMBptr->feuille._VECTptr->front()._VECTptr,N,D,num,den);
	  return true;
	}
      }
      n=equalposcomp(lv,e);
      if (n && (unsigned(n)<=lvnum.size())){
	num=lvnum[n-1];
	den=lvden[n-1];
	return true;
      }
      if ((!l.empty()) && (l.front().type==_VECT) ){
	int i,j;
	if (equalposmat(l,e,i,j)){
	  num=monomial2gen(monomial<gen>(gen(1),j+1,int(l[i]._VECTptr->size())));
	  for (int k=i-1;k>=0;--k)
	    num=monomial2gen(monomial<gen>(num,int(l[k]._VECTptr->size())));
	  den=plus_one;
	  return true;
	}
      }
      else {
	n=equalposcomp(l,e);
	if (n){
	  num=monomial2gen(monomial<gen>(gen(1),n,l_size));
	  den=plus_one;
	  return true;
	}
      }
      if (e.type!=_SYMB){
	num=e;
	den=plus_one;
	return true;
      }
      return sym2r(*e._SYMBptr,iext,l,lv,lvnum,lvden,l_size,num,den,contextptr);
    case _FRAC:
      return sym2r(*e._FRACptr,iext,l,lv,lvnum,lvden,l_size,num,den,contextptr);
    case _VECT:
      return sym2r(*e._VECTptr,iext,l,lv,lvnum,lvden,l_size,num,den,contextptr);
    case _POLY: case _EXT:
      if ((!l.empty()) && (l.front().type==_VECT) ){
	num=e;
	for (int k=int(l.size())-1;k>=0;--k) // was l_size
	  num=monomial2gen(monomial<gen>(num,int(l[k]._VECTptr->size())));
	den=plus_one;
      }
      else {
	num=monomial2gen(monomial<gen>(e,l_size));
	den=plus_one;
      }
      return true;
    case _MOD:
      return sym2rmod(e._MODptr,iext,l,lv,lvnum,lvden,l_size,num,den,contextptr);
    case _USER:
      num=e;
      den=plus_one;
      return true;
    default: 
#ifndef NO_STDEXCEPT
      settypeerr(gettext("Unable to handle type [sym2poly.cc]")+e.print(contextptr));
#endif
      num=gensizeerr(gettext("Unable to handle ")+e.print(contextptr));
      den=plus_one;
      return 0;
    }
    return 0;
  }

  static void extract_ext(const gen & g,gen & extracted,vector<int> & embeddings){
    if (g.type==_EXT){
      extracted= ext_reduce(g);
      return;
    }
    if (g.type==_POLY && !g._POLYptr->coord.empty() && Tis_constant<gen>(*g._POLYptr)){
      embeddings.push_back(g._POLYptr->dim);
      extract_ext(g._POLYptr->coord.front().value,extracted,embeddings);
    }
    else
      extracted=0;
  }

  static bool check_ext(const gen & oldext,const gen & curext){
    if (oldext.type!=_VECT)
      return false;
    if (curext.type==_EXT)
      return oldext!=*(curext._EXTptr+1);
    if (curext.type==_FRAC)
      return check_ext(oldext,curext._FRACptr->num);
    return false;
  }

  // rewrite extension inside a in terms of a common rootof
  // extensions in a must be at the top level
  // alg_extin is the current list of already translated algebraic extension
  // alg_extout is the current list of corresponding values
  // w.r.t the common minpoly
  // return a in terms of the common ext
  static vecteur common_algext(const vecteur & a,const vecteur & l,GIAC_CONTEXT){
    const_iterateur it,itend=a.end();
    vecteur alg_extin,alg_extoutnum,alg_extoutden;
#if 0 // ndef GIAC_HAS_STO38
    // trying with rational univariate rep.
    for (it=a.begin();it!=itend;++it){
      if (it->type==_EXT){
	gen minpoly=*(it->_EXTptr+1);
	if (minpoly.type!=_VECT)
	  break;
	unsigned i=0,s=minpoly._VECTptr->size();
	for (;i<s;++i){
	  if (!is_integer((*minpoly._VECTptr)[i]))
	    break;
	}
	if (i!=s)
	  break;
	alg_extin.push_back(minpoly);
      }
    }
    if (it==itend){
      // extract system of equations from alg_extin and call gbasis
      // factor minimal polynomial and find the correct factor
    }
    alg_extin.clear(); alg_extoutnum.clear(); alg_extoutden.clear();
#endif
    for (it=a.begin();it!=itend;++it){
      if (it->type==_EXT)
	break;
    }
    if (itend==it)
      return a;
    alg_extin.push_back(*(it->_EXTptr+1));
    gen curext=*(it->_EXTptr+1);
    gen minpoly=curext;
    alg_extoutnum.push_back(makevecteur(1,0));
    alg_extoutden.push_back(1);
    ++it;
    for (;it!=itend;++it){
      if (it->type!=_EXT)
	continue;
      curext=*(it->_EXTptr+1);
      if (equalposcomp(alg_extin,curext))
	continue;
      // make common extension of curext and minpoly
      gen oldminpoly=minpoly;
      gen oldcurext=curext;
      if (curext==oldminpoly){
	alg_extin.push_back(oldcurext);
	alg_extoutnum.push_back(makevecteur(1,0));
	alg_extoutden.push_back(1);
	continue;
      }
      else
	minpoly=common_EXT(curext,oldminpoly,&l,contextptr);
      if (minpoly.type!=_VECT)
	return vecteur(1,gensizeerr(contextptr));
      if (minpoly._VECTptr->size()>unsigned(MAX_COMMON_ALG_EXT_ORDER_SIZE))
	return vecteur(1,undef);
      // compute alg_extoutnum/den using newminpoly
      int s=int(alg_extin.size());
      for (int i=0;i<s;++i){
	if (alg_extoutnum[i].type!=_VECT)
	  return vecteur(1,gensizeerr(contextptr));
	if (oldminpoly.type==_FRAC){
	  gen resn,resd;
	  hornerfrac(*alg_extoutnum[i]._VECTptr,oldminpoly._FRACptr->num,oldminpoly._FRACptr->den,resn,resd);
	  alg_extoutden[i]=alg_extoutden[i]*resd;
	  if (resn.type!=_EXT || *(resn._EXTptr+1)!=minpoly)
	    return vecteur(1,gensizeerr(contextptr));
	  alg_extoutnum[i]=*resn._EXTptr;
	}
	else {
	  gen tmp=horner(*alg_extoutnum[i]._VECTptr,oldminpoly);
	  if (tmp.type!=_EXT || *(tmp._EXTptr+1)!=minpoly)
	    return vecteur(1,gensizeerr(contextptr));
	  alg_extoutnum[i]=*tmp._EXTptr;
	}
      }
      // add oldcurext/curext to the list
      alg_extin.push_back(oldcurext);
      if (curext.type==_FRAC){
	alg_extoutden.push_back(curext._FRACptr->den);
	curext=curext._FRACptr->num;
      }
      else
	alg_extoutden.push_back(1);
      if (curext.type!=_EXT || *(curext._EXTptr+1)!=minpoly)
	return vecteur(1,gensizeerr(contextptr));
      alg_extoutnum.push_back(*curext._EXTptr);
    }
    vecteur res;
    for (it=a.begin();it!=itend;++it){
      if (it->type!=_EXT)
	res.push_back(*it);
      int n=equalposcomp(alg_extin,*(it->_EXTptr+1));
      if (!n)
	return vecteur(1,gensizeerr(contextptr));
      --n;
      gen tmp=alg_extoutnum[n];
      if (tmp.type==_VECT)
	tmp.subtype=_POLY1__VECT;
      if (!is_one(alg_extoutden[n])){
	gen resn,resd;
	hornerfrac(*it->_EXTptr->_VECTptr,tmp,alg_extoutden[n],resn,resd);
	if (resn.type==_VECT && minpoly.type==_VECT && resn._VECTptr->size()>=minpoly._VECTptr->size()){
	  resn = *resn._VECTptr % *minpoly._VECTptr;
	  gen tmp;
	  lcmdeno_converted(*resn._VECTptr,tmp,contextptr);
	  resd=resd*tmp;
	}
	res.push_back(algebraic_EXTension(resn,minpoly)/resd);
      }
      else {
	tmp=horner(*it->_EXTptr,tmp);
	if (tmp.type!=_VECT)
	  res.push_back(tmp);
	else
	  res.push_back(algebraic_EXTension(tmp,minpoly));
      }
    }
    return res;
  }

  static bool compute_lv_lvnum_lvden(const vecteur & l,vecteur & lv,vecteur & lvnum,vecteur & lvden,bool & totally_converted,int l_size,GIAC_CONTEXT){
    gen num,den;
    // sort by ascending complexity
    iterateur it=lv.begin(),itend=lv.end();
    lvnum.reserve(itend-it);
    lvden.reserve(itend-it);
    gen_sort_f(it,itend,symb_size_less);
    bool algext=false;
    // find num/den for each var of lv
    for (;it!=itend;++it){
      algext = algext || (it->type==_SYMB && (it->_SYMBptr->sommet==at_pow || it->_SYMBptr->sommet==at_rootof)) ;
      totally_converted = totally_converted && sym2r(*it,0,l,lv,lvnum,lvden,l_size,num,den,contextptr);
      lvnum.push_back(num);
      lvden.push_back(den);
    }
    if (!algext || lv.size()==1 || l==lv)
      return true;
    it=lv.begin();
    // create common extensions for rootofs
    // first find extensions and embeddings levels
    std::map< int,vecteur> extensions,newext;
    for (int i=0;it!=itend;++it,++i){
      if (it->is_symb_of_sommet(at_pow) || it->is_symb_of_sommet(at_rootof)){
	gen ext;
	vector<int> emb;
	extract_ext(lvnum[i],ext,emb);
	if (ext.type==_FRAC){
	  lvnum[i]=lvnum[i]*ext._FRACptr->den;
	  lvden[i]=lvden[i]*ext._FRACptr->den;
	  ext=ext._FRACptr->num;
	}
	if (!is_zero(ext))
	  extensions[int(emb.size())].push_back(ext);
      }
    }
    // now rewrite each element of the list at each embedding level
    std::map< int,vecteur >::iterator jt=extensions.begin(),jtend=extensions.end();
    for (;jt!=jtend;++jt){
      if (is_undef( (newext[jt->first]=common_algext(jt->second,l,contextptr)) ))
	return false;
    }
    //stores cur
    it=lv.begin();
    for (int i=0;it!=itend;++it,++i){
      if (it->is_symb_of_sommet(at_pow) || it->is_symb_of_sommet(at_rootof)){
	gen ext;
	vector<int> emb;
	extract_ext(lvnum[i],ext,emb);
	if (is_zero(ext))
	  continue;
	int n=equalposcomp(extensions[int(emb.size())],ext);
	if (!n)
	  return false; // setsizeerr();
	gen tmp=newext[int(emb.size())],den=1;
	if (tmp.type!=_VECT || int(tmp._VECTptr->size())<n)
	  return false; // setsizeerr();
	tmp=(*tmp._VECTptr)[n-1];
	if (tmp.type==_FRAC){
	  den=tmp._FRACptr->den;
	  tmp=tmp._FRACptr->num;
	}
	for (;!emb.empty();){
	  tmp=polynome(tmp,emb.back());
	  den=polynome(den,emb.back());
	  emb.pop_back();
	}
	lvden[i]=lvden[i]*den;
	lvnum[i]=tmp;
      }
    }
    return true;
  }

  bool in_find_extension(const gen & extension,vecteur & already,vector<int> & embed,gen & iext,GIAC_CONTEXT){
    iext=0;
    if (extension.type==_POLY && !extension._POLYptr->coord.empty()){
      const polynome & p=*extension._POLYptr;
      embed.push_back(p.dim);
      for (unsigned i=0;i<p.coord.size();++i){
	if (in_find_extension(p.coord[i].value,already,embed,iext,contextptr))
	  return true;
      }
      embed.pop_back();
      return false;
    }
    if (extension.type==_FRAC)
      return in_find_extension(extension._FRACptr->num,already,embed,iext,contextptr);
    if (extension.type!=_EXT)
      return false;
    gen Extension=*(extension._EXTptr+1);
    if (Extension.type!=_VECT || equalposcomp(already,Extension))
      return false;
    bool done=false;
    for (unsigned i=0;i<Extension._VECTptr->size();++i){
      gen c=(*Extension._VECTptr)[i];
      if (!is_integer(c)){
	done=true;
	// recurse
	if (in_find_extension(c,already,embed,iext,contextptr))
	  return true;
      }
    }
    if (done)
      return false;
    iext=makevecteur(1,0,1);
    gen currentext=Extension;
    common_EXT(iext,currentext,0,contextptr);
    if (iext.type==_VECT)
      iext=algebraic_EXTension(makevecteur(1,0),iext);
    if (currentext.type==_EXT)
      currentext=*(currentext._EXTptr+1);
    Extension=change_subtype(Extension,_POLY1__VECT);
    currentext=change_subtype(currentext,_POLY1__VECT);
    if (Extension==currentext)
      return true;
    already.push_back(Extension);
    return false;
  }

  // remove embedded i in n
  bool clean_iext(gen & n,gen & d,const gen & iext,GIAC_CONTEXT){
    if (iext==0) return true;
    if (n.type==_POLY){
      polynome p=*n._POLYptr;
      gen iext_=iext;
      if (iext.type==_FRAC){
	if (iext._FRACptr->num.type==_POLY){
	  if (iext._FRACptr->num._POLYptr->dim!=0 || iext._FRACptr->num._POLYptr->coord.empty())
	    return false;
	  iext_=iext._FRACptr->num._POLYptr->coord.front().value/iext._FRACptr->den;
	}
      }
      else {
	if (iext.type==_POLY){
	  if (iext._POLYptr->dim!=0 || iext._POLYptr->coord.empty())
	    return false;
	  iext_=iext._POLYptr->coord.front().value;
	}
      }
      unsigned i=0;
      for (i=0;i<p.coord.size();++i){
	gen D=d;
	clean_iext(p.coord[i].value,D,iext_,contextptr);
	p.coord[i].value=p.coord[i].value/D;
      }
      d=1,
      lcmdeno(p,d);
      n=d*p;
      return true;
    }
    if (n.type==_EXT){
      gen n1=*n._EXTptr,n2=*(n._EXTptr+1);
      if (n1.type==_VECT){
	vecteur nv=*n1._VECTptr;
	if (has_i(nv)){
	  gen r=algebraic_EXTension(makevecteur(1,0),n2),R,I,res=0;
	  for (unsigned i=0;i<nv.size();++i){
	    res = res*r;
	    reim(nv[i],R,I,contextptr);
	    res += R+I*iext;
	  }
	  if (res.type==_FRAC){
	    d=d*res._FRACptr->den;
	    res=res._FRACptr->num;
	  }
	  n=res;
	}
      }
      return true;
    }
    if (n.type==_CPLX){
      n=(*n._CPLXptr)+(*(n._CPLXptr+1))*iext;
      gen N,D;
      fxnd(n,N,D);
      n=N;
      d=d*D;
      return true;
    }
    return true;
  }

  bool clean_iext(vecteur & lvnum,vecteur & lvden,const gen & iext,GIAC_CONTEXT){
    if (iext==0) return true;
    for (unsigned i=0;i<lvnum.size();++i){
      if (!clean_iext(lvnum[i],lvden[i],iext,contextptr))
	return false;
    }
    return true;
  }

  gen find_iext(const gen & e,const vecteur & lvnum,GIAC_CONTEXT){
    gen iext(0);
    if (has_i(e)){ 
      // check if i is inside algebraic extensions from lvnum
      vecteur already; unsigned k; 
      for (k=0;k<lvnum.size();++k){
	gen extension=lvnum[k];
	vector<int> embed;
	if (in_find_extension(extension,already,embed,iext,contextptr)){
	  gen iextnum,iextden;
	  fxnd(iext,iextnum,iextden);
	  for (;!embed.empty();){
	    iextnum=polynome(iextnum,embed.back());
	    iextden=polynome(iextden,embed.back());
	    embed.pop_back();
	  }
	  return iextnum/iextden;
	}
      }
    }
    return 0;
  }

  gen rootof_extract(const gen & e,GIAC_CONTEXT){
    if (e.type==_VECT && e._VECTptr->size()==2)
      return e._VECTptr->front()*symb_rootof(gen(makevecteur(1,0),_POLY1__VECT),e._VECTptr->back(),contextptr);
    return symbolic(at_rootof,e);
  }

  void lvar_rootof(const gen & e,const vecteur &l,vecteur & lv,GIAC_CONTEXT){
    if (!l.empty() && l.front().type==_VECT && has_op(e,*at_rootof)){
      vecteur lve(lvar(e)),lvr;
      for (int i=0;i<lve.size();++i){
	if (lve[i].is_symb_of_sommet(at_rootof)){
	  lvr.push_back(lve[i]);
	  continue;
	}
      }
      if (!lvr.empty()){
	vector<const unary_function_ptr *> vu;
	vu.push_back(at_rootof); 
	vector <gen_op_context> vv;
	vv.push_back(rootof_extract);
	gen er=subst(e,vu,vv,true,contextptr);
	lvar(er,lv);
      }
      else
	lvar(e,lv);
    }
    else
      lvar(e,lv);
  }

  bool sym2r (const gen &e,const vecteur &l, int l_size,gen & num,gen & den,GIAC_CONTEXT){
    if (e.type<_POLY){
      num=e;
      den=plus_one;
      return true; 
    }
    if ( (e.type==_POLY) || (e.type==_EXT)){
      if ((!l.empty()) && (l.front().type==_VECT) ){
	num=e;
	for (int k=int(l.size())-1;k>=0;--k) // was l.size()
	  num=monomial2gen(monomial<gen>(num,int(l[k]._VECTptr->size())));
	den=plus_one;
      }
      else {
	num=monomial2gen(monomial<gen>(e,l_size));
	den=plus_one;
      }
      return true;
    }
    bool totally_converted=true;
    vecteur lv,lvnum,lvden;
    lvar_rootof(e,l,lv,context0);
    if (!compute_lv_lvnum_lvden(l,lv,lvnum,lvden,totally_converted,l_size,contextptr)){
      num=undef;
      return false;
    }
    gen iext=find_iext(e,lvnum,contextptr);
    clean_iext(lvnum,lvden,iext,contextptr);
    totally_converted =totally_converted && sym2r(e,iext,l,lv,lvnum,lvden,l_size,num,den,contextptr);
    if (is_inf(num) && !is_inf(den)) den=1;
    if (is_inf(den) && !is_inf(num)) num=1;
    // If den is a _POLY, multiply den by the _EXT conjugate of it's lcoeff
    // FIXME this should be done recursively if the 1st coeff is a _POLY!
    if (den.type==_POLY && !den._POLYptr->coord.empty() && den._POLYptr->coord.front().value.type==_EXT){
      gen a = ext_reduce(den._POLYptr->coord.front().value);
      if (is_undef(a)) return false;
      if (a.type == _EXT && a._EXTptr->type==_VECT){
	vecteur u,v,d;
	egcd(*(a._EXTptr->_VECTptr),*((a._EXTptr+1)->_VECTptr),0,u,v,d);
	if (d.size()==1){
	  gen aconj=algebraic_EXTension(u,*(a._EXTptr+1));
	  aconj=polynome(aconj,int(den._POLYptr->coord.front().index.size()));
	  num=aconj*num;
	  den=aconj*den;
	  simplify3(num,den);
	}
      }
    }
    return totally_converted;
  }

  fraction sym2r(const gen & e, const vecteur & l,GIAC_CONTEXT){
    int l_size;
    if (!l.empty() && l.front().type==_VECT)
      l_size=int(l.front()._VECTptr->size());
    else
      l_size=int(l.size());
    gen num,den;
    bool ok=sym2r(e,l,l_size,num,den,contextptr);
    if (!ok){
      num=string2gen("Error in normal",false);
      num.subtype=-1; //gensizeerr(contextptr);
    }
    if (is_positive(-den,contextptr)) 
      return fraction(-num,-den);
    else
      return fraction(num,den);
  }

  // fraction / x -> fraction of vecteur
  gen e2r(const gen & e,const gen & x,GIAC_CONTEXT){
    vecteur l(1,x);
    lvar(e,l);
    gen r=polynome2poly1(e2r(e,l,contextptr),1);
    return r2e(r,cdr_VECT(l),contextptr);
  }

  gen e2r(const gen & e,const vecteur & l,GIAC_CONTEXT){
    if (e.type!=_VECT){
      return sym2r(e,l,contextptr);
    }
    bool totally_converted=true;
    int l_size;
    if (!l.empty() && l.front().type==_VECT)
      l_size=int(l.front()._VECTptr->size());
    else
      l_size=int(l.size());
    gen num,den;
    vecteur lv,lvnum,lvden;
    lvar_rootof(e,l,lv,contextptr);
    if (!compute_lv_lvnum_lvden(l,lv,lvnum,lvden,totally_converted,l_size,contextptr))
      return gensizeerr(contextptr);
    gen iext=find_iext(e,lvnum,contextptr);
    clean_iext(lvnum,lvden,iext,contextptr);
    vecteur res;
    const_iterateur jt=e._VECTptr->begin(),jtend=e._VECTptr->end();
    for (;jt!=jtend;++jt){
      sym2r(*jt,iext,l,lv,lvnum,lvden,l_size,num,den,contextptr);
      res.push_back(num/den);
    }
    return gen(res,e.subtype);
  }

  //**********************************
  // tensor to symbolic
  //**********************************

  static gen niceprod(ref_vecteur * res,bool negatif){
    gen tmp;
    if (res->v.size()!=1)
      tmp=symbolic(at_prod,gen(res,_SEQ__VECT));
    else {
      tmp=res->v.front();
      delete_ref_vecteur(res);
    }
    return negatif?-tmp:tmp;
  }

  gen r2sym(const gen & e,const index_m & i,const vecteur & l,GIAC_CONTEXT){
    if (is_undef(e))
      return e;
    if (i.size()!=l.size())
      return gensizeerr(gettext("sym2poly/r2sym(const gen & e,const index_m & i,const vecteur & l)"));
    vecteur::const_iterator l_it=l.begin();
    index_t::const_iterator it=i.begin(),itend=i.end();
    ref_vecteur * res=new_ref_vecteur(0);
    res->v.reserve(itend-it+1);
    bool negatif=false;
    if (!is_one(e) || (e.type==_MOD) ){
      if ( (e.type<=_REAL || e.type==_FRAC) && is_positive(-e,contextptr)){
	negatif=true;
	if (!is_minus_one(e))
	  res->v.push_back(-e);
      }
      else
	res->v.push_back(e);
    }
    for (;it!=itend;++it,++l_it){
      if (!*it)
        continue;
      if (*it==1){
        res->v.push_back(*l_it);
        continue;
      }
      if (l_it->type==_IDNT)
        res->v.push_back(new_ref_symbolic(symbolic(at_pow,gen(makenewvecteur(*l_it,*it),_SEQ__VECT))));  
      else 
	res->v.push_back(pow(*l_it,*it,contextptr)); // change for normal(abs(z)^2), was pow(*l_it,*it)
    }
    if (res->v.empty()){
      delete_ref_vecteur(res);
      return e;
    }
    return niceprod(res,negatif);
  }

  static gen r2sym2(const polynome & p, const vecteur & l,GIAC_CONTEXT){
    monomial_v::const_iterator it=p.coord.begin();
    monomial_v::const_iterator itend=p.coord.end();
    if (itend-it==1)
      return r2sym(it->value,it->index,l,contextptr);
    ref_vecteur * res=new_ref_vecteur(0);
    res->v.reserve(itend-it);
    for (;it!=itend;++it){
      res->v.push_back(r2sym(it->value,it->index,l,contextptr)) ;
    }
    if (res->v.size()==1){
      gen tmp=res->v.front();
      delete res;
      return tmp;
    }
    if (increasing_power(contextptr)) 
      reverse(res->v.begin(),res->v.end());
    return symbolic(at_plus,gen(res,_SEQ__VECT));
  }

  gen r2sym(const polynome & p, const vecteur & l,GIAC_CONTEXT){
    if (p.coord.empty())
      return 0;
    if (p.dim==0){
      return p.constant_term();
    }
    if (is_positive(-p.coord.front()))
      return -r2sym2(-p,l,contextptr);
    return r2sym2(p,l,contextptr);
  }

  gen r2sym(const fraction & f, const vecteur & l,GIAC_CONTEXT){
    if (f.den.type==_POLY && is_positive(-f.den._POLYptr->coord.front())){
      return rdiv(r2sym(-f.num,l,contextptr),r2sym(-f.den,l,contextptr),contextptr);
    }
    // workaround for e.g. normal(sqrt(3)*sqrt(15))
    gen fn=r2sym(f.num,l,contextptr);
    gen fd=r2sym(f.den,l,contextptr);
    if (fn.is_symb_of_sommet(at_prod) && fn._SYMBptr->feuille.type==_VECT && fd.type==_INT_ && absint(fd.val)>1){
      gen c=1;
      vecteur v(*fn._SYMBptr->feuille._VECTptr);
      fn=1;
      for (unsigned i=0;i<v.size();++i){
	if (v[i].type==_INT_){
	  c=c*v[i];
	  simplify(c,fd);
	}
	else
	  fn=fn*v[i];
      }
      fn=c*fn;
    }
    gen res=rdiv(fn,fd,contextptr);
    if (has_inf_or_undef(res))
      return fraction(fn,fd);
    return res;
  }

  gen r2sym(const vecteur & v,const vecteur & l,GIAC_CONTEXT){
    const_iterateur it=v.begin(),itend=v.end();
    ref_vecteur * res=new_ref_vecteur(0);
    res->v.reserve(itend-it);
    for (;it!=itend;++it)
      res->v.push_back(r2sym(*it,l,contextptr));
    return res;
  }

  static gen r2sym(const gen & p, const const_iterateur & lt, const const_iterateur & ltend,GIAC_CONTEXT);

  static gen solve_deg2(const gen & p,const gen & pmin,const const_iterateur & lt, const const_iterateur & ltend,GIAC_CONTEXT){
    if ( p.type!=_VECT || pmin.type!=_VECT)
      return gensizeerr(gettext("sym2poly.cc/ckdeg2_rootof"));
    vecteur & w = *pmin._VECTptr;
    int s=int(w.size());
    if (s!=3)// || is_greater(linfnorm(pmin,contextptr),(1<<30),contextptr))
      return symb_rootof(p,r2sym(pmin,lt,ltend,contextptr),contextptr);
    if (p._VECTptr->size()!=2)
      return gensizeerr(gettext("sym2poly.cc/ckdeg2_rootof"));  
    gen b_over_2=rdiv(w[1],plus_two,contextptr);
    if (is_zero(b_over_2))
      return p._VECTptr->front()*sqrt(r2sym(-w.back()/w.front(),lt,ltend,contextptr),contextptr)+p._VECTptr->back();
    gen x;
    if (b_over_2.type!=_FRAC){
      gen a=r2sym(w.front(),lt,ltend,contextptr);
      gen minus_b_over_2=r2sym(-b_over_2,lt,ltend,contextptr);
      gen delta_prime=r2sym(pow(b_over_2,2,contextptr)-w.front()*w.back(),lt,ltend,contextptr);
      x=rdiv(minus_b_over_2+sqrt(delta_prime,contextptr),a,contextptr);
    }
    else {
      gen two_a=r2sym(plus_two*w.front(),lt,ltend,contextptr);
      gen minus_b=r2sym(-w[1],lt,ltend,contextptr);
      gen delta=r2sym(w[1]*w[1]-gen(4)*w.front()*w.back(),lt,ltend,contextptr);
      x=rdiv(minus_b+sqrt(delta,contextptr),two_a,contextptr);
    }
    return p._VECTptr->front()*x+p._VECTptr->back();
  }

  /*
  static gen ckdeg2_rootof(const gen & p,const gen & pmin,GIAC_CONTEXT){
    if ( p.type!=_VECT || pmin.type!=_VECT)
      return gensizeerr(gettext("sym2poly.cc/ckdeg2_rootof"));
    if (pmin._VECTptr->size()!=3)
      return symb_rootof(p,pmin,contextptr);
    if (p._VECTptr->size()!=2)
      return gensizeerr(gettext("sym2poly.cc/ckdeg2_rootof"));   
    vecteur v;
    identificateur x(" x");
    in_solve(symb_horner(*pmin._VECTptr,x),x,v,1,contextptr); 
    if (v.empty())
      return gensizeerr(gettext("No root found for pmin"));
    return p._VECTptr->front()*v.front()+p._VECTptr->back();
  }
  */

  // check that an _EXT e is root of a second order poly
  // return 0 if not, 2 if pmin(e) is 2nd order, 1 otherwise
  // FIXME: if pmin has integer coeffs as well as e
  // translate so that we just need to find the sign
  // then we are reduced to find the sign of P(alpha) where alpha is the
  // largest real root of a polynomial Q
  // Using VAS [csturm.cc: gen complexroot(const gen & g,bool complexe,GIAC_CONTEXT)]
  // find isolation interval for roots of Q, select the largest one,
  // find isolation interval for roots of P, if they intersect with previous one
  // improve precision by dichotomy until sign is resolved
  static int is_root_of_deg2(const gen & e,vecteur & v,GIAC_CONTEXT){
    // find a,b,c such that a*e*e+b*e+c=0
#ifdef DEBUG_SUPPORT
    if (e.type!=_EXT || e._EXTptr->type!=_VECT)
      setsizeerr(gettext("sym2poly.cc/is_root_of_deg2"));
#endif // DEBUG_SUPPORT
    gen pmin=*(e._EXTptr+1),value;
    if (has_rootof_value(pmin,value,contextptr))
      return 0;
    vecteur vpmin(min_pol(pmin));
    if (!vpmin.empty() && is_undef(vpmin))
      return 0;
    if (vpmin.size()==3) 
      return 2;
    v.clear();
    gen e_square(e*e);
    if (e_square.type!=_EXT){ // b=0, a=1, e*e=-c
      v.push_back(plus_one);
      v.push_back(zero);
      v.push_back(-e_square);
      return 1;
    }
    if (e_square._EXTptr->type!=_VECT)
      return 0; // setsizeerr(gettext("sym2poly.cc/is_root_of_deg2"));
    int s=int(e._EXTptr->_VECTptr->size());      
    int s2=int(e_square._EXTptr->_VECTptr->size());
    if (s!=s2)
      return 0;
    gen b=-e_square._EXTptr->_VECTptr->front();
    gen a=e._EXTptr->_VECTptr->front();
    simplify(a,b);
    gen c=a*e_square+b*e;
    if (c.type==_EXT)
      return 0;
    v.push_back(a);
    v.push_back(b);
    v.push_back(-c);
    return 1;
  }

  static gen randassume(const vecteur & boundaries,GIAC_CONTEXT){
    if (boundaries.empty())
      return 0;
    if (boundaries[0].type==_VECT)
      return randassume(*boundaries[0]._VECTptr,contextptr);
    if (boundaries.size()<2)
      return 0;
    gen a=boundaries[0],b=boundaries[1];
    if (a==minus_inf){
      if (b==plus_inf)
	return _rand(makesequence(-100,100),contextptr);
      return _rand(makesequence(b-100,b-1),contextptr);
    }
    if (b==plus_inf)
      return _rand(makesequence(a+1,a+100),contextptr);
    return (b-a)*_rand(makesequence(0.0,1.0),contextptr)+a;
  }

  static void check_assume(vecteur & vzero,const vecteur & vassume,GIAC_CONTEXT){
    for (unsigned i=0;i<vzero.size();++i){
      gen assi=vassume[i],tmp;
      if (assi.type==_IDNT && has_evalf(assi,tmp,1,contextptr)){
	vzero[i]=tmp;
	continue;
      }
      if (assi.type==_VECT && assi.subtype==_ASSUME__VECT && assi._VECTptr->size()==3){
	if((*assi._VECTptr)[1].type==_VECT && !(*assi._VECTptr)[1]._VECTptr->empty()){
	  vzero[i]=randassume(*(*assi._VECTptr)[1]._VECTptr,contextptr);
	}
      }
    }
  }

  gen minimal_polynomial(const gen & pp,bool minonly,GIAC_CONTEXT){
    gen f=*(pp._EXTptr+1);
    if (f.type!=_VECT)
      return undef;
    int d=int(f._VECTptr->size());
    gen r=evalf_double(pp,1,contextptr);
#ifdef HAVE_LIBMPFR
    if (is_undef(r))
      r=accurate_evalf(pp,256);
#endif
    if (is_undef(r))
      *logptr(contextptr) << "Unable to eval numerically, root choice may be wrong\n";
    matrice m(d);
    m[0]=vecteur(d-1);
    m[0]._VECTptr->back()=1;
    gen ppi(1);
    for (int i=1;i<d;++i){
      ppi=ppi*pp;
      if (ppi.type!=_EXT){
	m[i]=vecteur(d-1);
	m[i]._VECTptr->back()=ppi;
      }
      else {
	m[i]=*ppi._EXTptr;
	if (m[i].type!=_VECT)
	  return gensizeerr(contextptr);
	m[i]=*m[i]._VECTptr;
	if (int(m[i]._VECTptr->size())<d-1)
	  *m[i]._VECTptr=mergevecteur(vecteur(d-1-m[i]._VECTptr->size()),*m[i]._VECTptr);
      }
    } // end for i
    if (!ckmatrix(m))
      return gensizeerr(contextptr);
    m=mtran(m); // d-1 rows, d columns
    int st=step_infolevel(contextptr);
    step_infolevel(contextptr)=0;
    vecteur mk=mker(m,contextptr);
    step_infolevel(contextptr)=st;
    gen pm(mk.front()); // min poly= 1st kernel element
    if (pm.type==_VECT){
      mk=*pm._VECTptr;
      reverse(mk.begin(),mk.end());
      mk=trim(mk,0);
      if (minonly){
	if (is_positive(-mk.front(),contextptr))
	  return gen(-mk,_POLY1__VECT);
	return gen(mk,_POLY1__VECT);
      }
      if ((d=int(mk.size()))<=5 && d!=4){
	identificateur x(" x");
	vecteur w;
	in_solve(symb_horner(mk,x),x,w,1,contextptr);
	if (r.type!=_DOUBLE_ && r.type!=_CPLX && r.type!=_REAL && !w.empty()) return w.front();
	for (unsigned k=0;k<w.size();++k){
	  if (lop(w[k],at_rootof).empty()){
	    gen wkd=evalf_double(w[k],1,contextptr);
      if (r.type==_REAL)
        wkd=accurate_evalf(w[k],256);
	    if (wkd!=w[k] && is_greater(1e-6,abs(1-r/wkd,contextptr),contextptr))
	      return w[k];
	  }
	}
      }
    }
    return undef;
  }

  gen accurate_evalf_until(const gen & g_,GIAC_CONTEXT){
#ifdef HAVE_LIBMPFR
    gen g(g_);
    int n=32;
    gen cur=evalf_double(g,1,contextptr);
    if (g.type==_EXT){
      gen p=*g._EXTptr;
      gen x=symb_rootof(makevecteur(1,0),*(g._EXTptr+1),contextptr);
      for (;n<=1024;n*=2){
	gen tmp=_evalf(makesequence(x,n),contextptr);
	tmp=_horner(makesequence(p,tmp),contextptr);
	gen test=(1-cur/tmp);
	if (!is_undef(test) && !is_inf(test) && is_greater(1e-12,test,contextptr))
	  return cur;
	cur=tmp;
      }
    }
    for (;n<=1024;n*=2){
      gen tmp=_evalf(makesequence(g,n),contextptr);
      gen test=(1-cur/tmp);
      if (!is_undef(test) && !is_inf(test) && is_greater(1e-12,test,contextptr))
	return cur;
      cur=tmp;
    }
    return cur;
#else
    return evalf_double(g_,1,contextptr);
#endif
  }

  static gen r2sym(const gen & p, const const_iterateur & lt, const const_iterateur & ltend,GIAC_CONTEXT){
    // Note that if p.type==_FRAC && p._FRACptr->num.type==_EXT
    // it might require another simplification with the denom
    if (p.type==_FRAC){
      gen res;
      if (p._FRACptr->den.type==_CPLX)
	res=rdiv(r2sym(p._FRACptr->num*conj(p._FRACptr->den,contextptr),lt,ltend,contextptr),r2sym(p._FRACptr->den*conj(p._FRACptr->den,contextptr),lt,ltend,contextptr),contextptr);
      else
	res=rdiv(r2sym(p._FRACptr->num,lt,ltend,contextptr),r2sym(p._FRACptr->den,lt,ltend,contextptr),contextptr);
      return (p._FRACptr->num.type==_EXT)?ratnormal(res,contextptr):res;
    }
    if (p.type==_MOD)
      return makemodquoted(r2sym(*p._MODptr,lt,ltend,contextptr),r2sym(*(p._MODptr+1),lt,ltend,contextptr));
    if (p.type==_VECT){
      const_iterateur it=p._VECTptr->begin(),itend=p._VECTptr->end();
      vecteur res;
      res.reserve(itend-it);
      for (;it!=itend;++it)
	res.push_back(r2sym(*it,lt,ltend,contextptr));
      return res;
    }
    if (p.type==_EXT){
      gen pp=ext_reduce(p);
      if (pp.type!=_EXT){
	if (is_undef(pp)) return pp;
	return r2sym(pp,lt,ltend,contextptr);
      }
      gen f=*(pp._EXTptr+1),fvalue;
#if 1
      if ( ( ltend==lt || (ltend-lt==1 && lt->type==_VECT && lt->_VECTptr->empty()) ) 
	   && f.type==_VECT && !has_rootof_value(f,fvalue,contextptr) && !keep_algext(contextptr) ){
	// univariate case
	// find minimal poly of the whole _EXT if extension degree is > 2
	int d=int(f._VECTptr->size());
	// FIXME remove d<=10, requires better handling of rref with Gauss integers
	if (d>3 && d<=10){
	  gen res=minimal_polynomial(pp,false,contextptr);
	  if (!is_undef(res))
	    return res;
	} // end if d>3
      }
#endif
      if ((pp._EXTptr+1)->type!=_VECT) 
	f=min_pol(*(pp._EXTptr+1)); // f is a _VECT
      if (is_undef(f))
	return f;
      if (keep_algext(contextptr)){
	gen value=r2sym(*pp._EXTptr,lt,ltend,contextptr);
	f=r2sym(f,lt,ltend,contextptr);
	return algebraic_EXTension(value,f);
      }
      if (f._VECTptr->size()==3){
	gen value;
	if (!has_rootof_value(f,value,contextptr))
	  return solve_deg2(r2sym(*(pp._EXTptr),lt,ltend,contextptr),f,lt,ltend,contextptr);
      }
      vecteur v;
      int t=0;
      fvalue=undef;
      if (!has_rootof_value(f,fvalue,contextptr))
	t=is_root_of_deg2(pp,v,contextptr);
      // if (t==2) return solve_deg2(r2sym(*(pp._EXTptr),lt,ltend,contextptr),f,lt,ltend,contextptr);
      // if (t && f==v) t=0;
      if (t==1){
	vecteur w;
	identificateur x(" x");
	in_solve(symb_horner(*(r2sym(v,lt,ltend,contextptr)._VECTptr),x),x,w,1,contextptr); 
	if (w.size()==1)
	  return w.front();
#ifndef NO_STDEXCEPT
	try {
#endif
	  vecteur vinit(lt,ltend);
	  if (lt!=ltend && vinit.front().type==_VECT)
	    vinit=*vinit.front()._VECTptr;
	  vecteur vassume(vinit);
	  for (unsigned i=0;i<vassume.size();++i){
	    if (vinit[i].type==_IDNT)
	      vinit[i]._IDNTptr->in_eval(0,vinit[i],vassume[i],contextptr);
	  }
	  gen vinitd;
	  vecteur vzero=vecteur(vinit.size());
	  bool tst=has_evalf(vinit,vinitd,1,contextptr);
	  if (tst)
	    vzero=*vinitd._VECTptr;
	  else 
	    check_assume(vzero,vassume,contextptr);
	  vzero=subst(vzero,vinit,vzero,false,contextptr);
	  gen tmp0=r2sym(*pp._EXTptr,lt,ltend,contextptr);
	  gen tmp1=r2sym(f,lt,ltend,contextptr);
	  for (int ntry=0;ntry<10;++ntry,tst=false){
	    gen tmp00=subst(tmp0,vinit,vzero,false,contextptr);
	    gen tmp10=subst(tmp1,vinit,vzero,false,contextptr);
	    if (!lvar(makevecteur(tmp00,tmp10)).empty())
	      break;
	    if (tmp10.type==_VECT && _size(_gcd(makesequence(tmp10,derivative(*tmp10._VECTptr)),contextptr),contextptr)>1)
	      tmp00=plus_one_half; // retry
	    else {
	      tmp00=algebraic_EXTension(tmp00,tmp10);
	      tmp00=accurate_evalf_until(tmp00,contextptr); // tmp00.evalf_double(1,contextptr); 
	    }
	    if (tmp00.type<=_CPLX){
	      gen tmp3=eval(subst(w.front(),vinit,vzero,false,contextptr),1,contextptr);
	      tmp3=accurate_evalf_until(tmp3,contextptr); // tmp3.evalf_double(1,contextptr); 
	      gen tmp2=eval(subst(w.back(),vinit,vzero,false,contextptr),1,contextptr);
	      tmp2=accurate_evalf_until(tmp2,contextptr);// tmp2.evalf_double(1,contextptr); 
	      if (tmp3.type<=_CPLX && tmp2.type<=_CPLX && tmp2!=tmp3){
		if (!vzero.empty() && !tst)
		  *logptr(contextptr) << gettext("Warning, choosing root of ") << f << gettext(" at parameters values ") << vzero << "\n";
		if (is_greater(abs(tmp3-tmp00,contextptr),abs(tmp2-tmp00,contextptr),contextptr))
		  return w.back();
		else
		  return w.front();
	      }
	    }
	    // tmp2 and tmp3 are identical or tmp0 is not real, retry
	    vzero=vranm(int(vzero.size()),0,contextptr);
	    check_assume(vzero,vassume,contextptr);
	  }
#ifndef NO_STDEXCEPT
	}
	catch (std::runtime_error & e){
	  last_evaled_argptr(contextptr)=NULL;
	  CERR << "sym2poly exception caught " << e.what() << "\n";
	}
#endif
	return w.front();
	/* gen tmp0=algebraic_EXTension(r2sym(*pp._EXTptr,lt,ltend),r2sym(f,lt,ltend)).evalf_double();
	if (tmp0.type==_DOUBLE_){
	  gen tmp1=evalf_double(w.front()),tmp2=evalf_double(w.back());
	  if (tmp1.type==_DOUBLE_ && tmp2.type==_DOUBLE_ && fabs((tmp2-tmp0)._DOUBLE_val)<fabs((tmp1-tmp0)._DOUBLE_val))
	    return w.back();
	    }
	    return w.front(); */
      }
      int s=int(f._VECTptr->size());
      v=vecteur(s,zero);
      v.front()=plus_one;
      v.back()=f._VECTptr->back();
      if (f==v){
	if (is_undef(fvalue))
	  fvalue=pow(r2sym(-v.back(),lt,ltend,contextptr),fraction(1,s-1),contextptr);
      }
      else
	return symb_rootof(r2sym(*(pp._EXTptr),lt,ltend,contextptr),r2sym(f,lt,ltend,contextptr),contextptr);
      return symb_horner(*(r2sym(*(pp._EXTptr),lt,ltend,contextptr)._VECTptr),fvalue);
    }
    if (p.type==_SPOL1){
      sparse_poly1 s=*p._SPOL1ptr;
      sparse_poly1::iterator it=s.begin(),itend=s.end();
      vecteur l(lt,ltend);
      for (;it!=itend;++it){
	it->coeff=r2sym(it->coeff,l,contextptr);
      }
      return s;
    }
    if ((p.type!=_POLY) || (lt==ltend))
      return p;
    if (p._POLYptr->coord.empty())
      return zero;
    if (p._POLYptr->dim==0){
      return r2sym(p._POLYptr->coord.front().value,lt+1,ltend,contextptr);
    }
    if (is_positive(-p,contextptr) && !is_positive(p,contextptr)) 
      return -r2sym(-p,lt,ltend,contextptr);
    monomial_v::const_iterator it=p._POLYptr->coord.begin();
    monomial_v::const_iterator itend=p._POLYptr->coord.end();
    if (itend==it)
      return zero;
    vecteur res;
    res.reserve(itend-it);
    for (;it!=itend;++it){
      res.push_back(r2sym(r2sym(it->value,lt+1,ltend,contextptr),it->index,*(lt->_VECTptr),contextptr)) ;
    }
    if (res.size()==1)
      return res.front();
    if (increasing_power(contextptr)) 
      reverse(res.begin(),res.end());
    return symbolic(at_plus,gen(res,_SEQ__VECT));
  }

  gen r2sym(const gen & p, const vecteur & l,GIAC_CONTEXT){
    if (p.type==_VECT){
      gen res=r2sym(*p._VECTptr,l,contextptr);
      if (res.type==_VECT)
	res.subtype=p.subtype;
      return res;
    }
    if ( (!l.empty()) && (l.front().type==_VECT) )
      return r2sym(p,l.begin(),l.end(),contextptr);
    if (p.type==_FRAC)
      return r2sym(*p._FRACptr,l,contextptr);
    if (p.type==_MOD)
      return makemodquoted(r2sym(*p._MODptr,l,contextptr),r2sym(*(p._MODptr+1),l,contextptr));
    if (p.type==_EXT){ 
      gen pp=ext_reduce(*(p._EXTptr),*(p._EXTptr+1));
      if (pp.type!=_EXT){
	if (is_undef(pp)) return pp;
	return r2sym(pp,l,contextptr);
      }
      gen f=*(pp._EXTptr+1);
      if ((pp._EXTptr+1)->type!=_VECT) 
	f=min_pol(*(pp._EXTptr+1)); // f is a _VECT
      if (is_undef(f))
	return f;
      vecteur v;
      int t=is_root_of_deg2(pp,v,contextptr);
      if (f._VECTptr->size()==3){
	return solve_deg2(r2sym(*(pp._EXTptr),l,contextptr),f,l.begin(),l.end(),contextptr);
      }
      if (t==2){
	// return ckdeg2_rootof(r2sym(*(pp._EXTptr),l,contextptr),r2sym(f,l,contextptr),contextptr);
	return solve_deg2(r2sym(*(pp._EXTptr),l,contextptr),f,l.begin(),l.end(),contextptr);
      }
      if (t==1){
	vecteur w;
	identificateur x(" x");
	in_solve(symb_horner(*(r2sym(v,l,contextptr)._VECTptr),x),x,w,1,contextptr);
	// find the nearest root from pp
	gen ww,ppp,mindist,curdist;
	if (has_evalf(w,ww,1,contextptr) && has_evalf(pp,ppp,1,contextptr)){
	  int pos=0;
	  vecteur & www = *ww._VECTptr;
	  mindist=abs(www[0]-ppp,contextptr);
	  for (int i=1;i<(int)www.size();++i){
	    curdist=abs(www[i]-ppp,contextptr);
	    if (is_strictly_greater(mindist,curdist,contextptr)){
	      pos=i;
	      mindist=curdist;
	    }
	  }
	  return w[pos];
	}
	return w.front();
      }
      int s=int(f._VECTptr->size());
      v=vecteur(s,zero);
      v.front()=plus_one;
      v.back()=f._VECTptr->back();
      gen theta;
      if (f==v)
	theta=pow(r2sym(-v.back(),l,contextptr),fraction(1,s-1),contextptr);
      else
	return symb_rootof(r2sym(*(pp._EXTptr),l,contextptr),r2sym(f,l,contextptr),contextptr);
      return symb_horner(*(r2sym(*(pp._EXTptr),l,contextptr)._VECTptr),theta);
    }
    if (p.type==_POLY)
      return r2sym(*p._POLYptr,l,contextptr);
    if (p.type==_SPOL1)
      return r2sym(p,l.begin(),l.end(),contextptr);
    return p;
  }

  gen r2e(const gen & p, const vecteur & l,GIAC_CONTEXT){
    return r2sym(p,l,contextptr);
  }

  // alias vecteur -> polynome / x
  gen r2e(const gen & r,const gen & x,GIAC_CONTEXT){
    if (r.type==_FRAC)
      return fraction(r2e(r._FRACptr->num,x,contextptr),r2e(r._FRACptr->den,x,contextptr));
    if (r.type==_VECT)
      return symb_horner(*r._VECTptr,x);
    else
      return r;
  }

  // convert factorization to symbolic form 
  gen r2sym(const factorization & vnum,const vecteur & l,GIAC_CONTEXT){
    gen resnum(1);
    factorization::const_iterator it=vnum.begin(),itend=vnum.end();
    for (;it!=itend;++it){
      // insure no embedded sqrt inside
      polynome p=it->fact;
      if (l.size()==1){
	vector< monomial<gen> >::iterator pt=p.coord.begin(),ptend=p.coord.end();
	vecteur vtmp(1,vecteur(0));
	for (;pt!=ptend;++pt){
	  pt->value=r2sym(pt->value,vtmp,contextptr);
	}
      }
      gen tmp=r2sym(gen(p),l,contextptr);
      resnum=resnum*pow(tmp,it->mult);
    }
    return resnum;
  }

  // convert pfde_VECT to symbolic form 
  gen r2sym(const vector< pf<gen> > & pfde_VECT,const vecteur & l,GIAC_CONTEXT){
    gen res(0);
    vector< pf<gen> >::const_iterator it=pfde_VECT.begin(),itend=pfde_VECT.end();
    for (;it!=itend;++it){
      res=res+rdiv(r2sym(gen(it->num),l,contextptr),(r2sym(gen(it->den/pow(it->fact,it->mult)),l,contextptr)*pow(r2sym(gen(it->fact),l,contextptr),it->mult)),contextptr);
    }
    return res;
  }

  //*****************************
  /* Fonctions relatives to ex */
  //*****************************

  // special version of lop(.,at_pow) that removes integral powers
  static vecteur lop_pow(const gen & g){
    if (g.type==_SYMB) {
      if (g._SYMBptr->sommet==at_pow && g._SYMBptr->feuille._VECTptr->back().type!=_INT_) 
	return vecteur(1,g);
      else {
	if (g._SYMBptr->sommet!=at_ln)
	  return lop_pow(g._SYMBptr->feuille);
      }
    }
    if (g.type!=_VECT)
      return vecteur(0);
    vecteur res;
    const_iterateur it=g._VECTptr->begin(),itend=g._VECTptr->end();
    for (;it!=itend;++it){
      gen cur=*it;
      if (cur.is_symb_of_sommet(at_pow) && cur._SYMBptr->feuille._VECTptr->back().type==_INT_)
	cur=cur._SYMBptr->feuille._VECTptr->front(); // was continue, changed for integrate((b^2*x^2 + a*b*x*2 + a^2)^(-3/2) , x) ;
      res=mergeset(res,lop_pow(cur));
    }
    return res;
  }

  // sqrt(g), check whether g is a+b+/-2*sqrt(a*b)
  static bool is_sqrtxy(const gen & g_,gen & res,GIAC_CONTEXT){
    gen g(g_);
    vecteur l=lop_pow(g);
    if (l.size()==2 && l[0]._SYMBptr->feuille[1]==plus_one_half && l[1]._SYMBptr->feuille[1]==plus_one_half){
      gen c,d,e,f;
      if (!is_linear_wrt(g,l[0],c,d,contextptr))
	return false;
      // c*l[0]+d
      if (!is_constant_wrt(d,l[1],contextptr))
	return false;
      if (!is_linear_wrt(c,l[1],e,f,contextptr) || !is_zero(f,contextptr))
	return false;
      // rewrite g 
      l=vecteur(1,symb_pow(l[0]._SYMBptr->feuille[0]*l[1]._SYMBptr->feuille[0],plus_one_half));
      g=d+e*l[0];
    }
    if (l.size()!=1) // improve for sqrt(a)*sqrt(b)
      return false;
    gen e(l[0]._SYMBptr->feuille);
    if (e.type!=_VECT || e._VECTptr->size()!=2 || e._VECTptr->back()!=plus_one_half)
      return false;
    e=e._VECTptr->front();
    gen c,d;
    if (!is_linear_wrt(g,l[0],c,d,contextptr))
      return false;
    e=e*(c*c/gen(4));
    gen cs=sign(c,contextptr);
    // d=a+b, e=a*b, factor x^2-d*x+e
    l=lidnt(makevecteur(d,e));
    gen x;
    for (;;){
      x=identificateur("x"+print_INT_(giac_rand(contextptr)));
      if (!equalposcomp(l,x))
	break;
    }
    gen f=_numer(x*x-d*x+e,contextptr);
    f=factor(f,x,false,contextptr);
    if (f.type!=_SYMB)
      return false;
    if (f._SYMBptr->sommet!=at_prod)
      return false;
    f=f._SYMBptr->feuille;
    if (f.type!=_VECT || f._VECTptr->size()!=2)
      return false;
    if (!is_linear_wrt(f._VECTptr->front(),x,c,e,contextptr) || is_zero(c,contextptr))
      return false;
    gen a=-e/c;
    if (!is_linear_wrt(f._VECTptr->back(),x,c,e,contextptr) || is_zero(c,contextptr))
      return false;
    gen b=-e/c;
    if (!is_greater(a,b,contextptr))
      swapgen(a,b);
    res=sqrt(a,contextptr)+cs*sqrt(b,contextptr);
    if (!is_greater(a,b,contextptr))
      *logptr(contextptr) << "Warning, assuming " << a-b << " is positive. Hint: run assume to make assumptions on a variable\n";
    return true;
  }

  static gen inner_sqrt(const gen & g){
    //return g; // disabled
    if (g.type==_VECT){
      vecteur v(*g._VECTptr);
      for (int i=0;i<v.size();++i)
	v[i]=inner_sqrt(v[i]);
      return gen(v,g.subtype);
    }
    if (g.type!=_SYMB)
      return g;
    gen f(inner_sqrt(g._SYMBptr->feuille));
    if (g._SYMBptr->sommet!=at_prod || f.type!=_VECT)
      return symbolic(g._SYMBptr->sommet,f);
    const vecteur & v=*f._VECTptr;
    gen G=1;
    vecteur res;
    for (int i=0;i<v.size();++i){
      if (v[i].is_symb_of_sommet(at_pow) && v[i]._SYMBptr->feuille[1]==plus_one_half){
	gen vi=v[i]._SYMBptr->feuille[0];
	if (vi.type<=_CPLX){
	  G=G*vi;
	  continue;
	}
	vi=G*vi;
	G=1;
	res.push_back(symbolic(at_pow,makesequence(vi,plus_one_half)));
      }	
      else
	res.push_back(v[i]);
    }
    if (!is_one(G))
      res.push_back(symbolic(at_pow,makesequence(G,plus_one_half)));
    if (res.empty())
      return 1;
    if (res.size()==1)
      return res.front();
    return symbolic(at_prod,gen(res,_SEQ__VECT));
  }

  gen rewrite_exp_integer(const gen & e,GIAC_CONTEXT){
    vecteur v(lop(e,at_exp));
    if (v.size()<2)
      return e;
    gen g;
    for (unsigned i=0;i<v.size();++i)
      g=gcd(g,v[i]._SYMBptr->feuille); // gcd of arg of exp
    vecteur w(v);
    for (unsigned i=0;i<v.size();++i){
      gen r=ratnormal(v[i]._SYMBptr->feuille/g,contextptr);
      if (r.type==_INT_ && absint(r.val)<=INT_KARAMUL_SIZE){
        if (r.val!=1)
          w[i]=symbolic(at_pow,makesequence(symb_exp(g),r));
        // if r==1 we keep w[i] unchanged but we do not erase it for the final check list size
      }
      else {
        v.erase(v.begin()+i);
        w.erase(w.begin()+i);
        --i;
      }
    }
    return (v==w || v.size()==1) ? e : subst(e,v,w,false,contextptr);
  }

  static gen in_normalize_sqrt(const gen & e,vecteur & L,bool keep_abs,GIAC_CONTEXT){
    if (complex_mode(contextptr) || has_i(e)) 
      return e;
    // remove multiple factors inside sqrt
    // vecteur l0=lop(e,at_pow),lin,lout;
    vecteur l0=lop_pow(e),lin,lout,lspow,larg;
    const_iterateur it=l0.begin(),itend=l0.end();
    for (;it!=itend;++it){
      vecteur & arg=*it->_SYMBptr->feuille._VECTptr;
      gen g=arg[1],expnum,expden;
      if (g.type==_FRAC){
	expnum=g._FRACptr->num;
	expden=g._FRACptr->den;
      }
      else {
	if ( (g.type!=_SYMB) || (g._SYMBptr->sommet!=at_prod) )
	  continue;
	gen & arg1=g._SYMBptr->feuille;
	if (arg1.type!=_VECT)
	  continue;
	vecteur & v=*arg1._VECTptr;
	if ( (v.size()!=2) || (v[1].type!=_SYMB) || (v[1]._SYMBptr->sommet==at_inv) )
	  continue;
	expnum=v[0];
	expden=v[1]._SYMBptr->feuille;
      }
      if ( (expden.type!=_INT_) || (expden.val!=2 ) )
	continue;
      if (is_one(expnum) && is_integer(arg[0]))
	continue;
      if (is_one(expnum) && arg[0].type==_SYMB){
	gen tmp=sqrt(arg[0],contextptr); // detect e.g. (1+sin(x))^(1/2)
	if (tmp!=*it){
	  lin.push_back(*it);
	  lout.push_back(tmp);
	  continue;
	}
      }
      // sqrt(arg[0]), we may check that arg[0] is a+b+/-2*sqrt(a*b)
      gen var,a,b,hyp;
      if (is_sqrtxy(arg[0],a,contextptr)){
	lin.push_back(*it);
	lout.push_back(a);
	continue;
      }
      var=ggb_var(arg[0]);
      // if var is not assigned and arg[0] depends linearly on var, add an assumption
      if (complex_mode(contextptr)==false && var.type==_IDNT && var._IDNTptr->eval(1,var,contextptr)==var){
	if (is_linear_wrt(arg[0],var,a,b,contextptr) && !is_zero(a)){
	  if (is_strictly_positive(a,contextptr))
	    hyp=symbolic(at_superieur_egal,makesequence(var,-b/a));
	  if (is_strictly_positive(-a,contextptr))
	    hyp=symbolic(at_inferieur_egal,makesequence(var,-b/a));
	  if (!is_zero(hyp))
	    L.push_back(hyp);
	}
      }
      vecteur lv(alg_lvar(arg[0]));
      gen num,den;
      a=e2r(arg[0],lv,contextptr);
      fxnd(a,num,den);
      simplify3(num,den);
      gen nd=num*den,out(1);
      gen nover2=rdiv(expnum,plus_two,contextptr);      
      if (nd.type==_EXT && nd._EXTptr->type==_VECT){ // extract content
	gen tmp=lgcd(*nd._EXTptr->_VECTptr);
	out=r2e(nd/tmp,lv,contextptr); 
	// removed the ^ to avoid larger extensions e.g. for normal(sin(pi/5))
	nd=tmp;
      }
      if (nd.type==_INT_ || nd.type==_ZINT){
	gen simpl,doubl;
	bool pos;
	zint2simpldoublpos(nd,simpl,doubl,pos,2,contextptr);
	if (!pos) simpl=-simpl;
	lin.push_back(*it);
	gen tmparg=r2e(den,lv,contextptr);
	if (keep_abs)
	  tmparg=abs(tmparg,contextptr);
	lout.push_back(pow(doubl/tmparg,expnum,contextptr)*pow(simpl*out,nover2,contextptr));
	continue;
      }
      if (nd.type!=_POLY)
	continue;
      lin.push_back(*it);      
      const factorization & f=rsqff(*nd._POLYptr);
      polynome s(plus_one,nd._POLYptr->dim),d(plus_one,s.dim);
      factorization::const_iterator jt=f.begin(),jtend=f.end();
      for (;jt!=jtend;++jt){
	if (jt->mult%2)
	  s=s*jt->fact;
	d=d*pow(jt->fact,jt->mult/2);
      }
      // Extract integer content of s
      gen cont=Tppz<gen>(s);
      if (s.dim==0 && !s.coord.empty() && is_positive(-s.coord[0].value,contextptr)){
	cont=-cont; s=-s;
      }
      if (s.dim && !larg.empty()){
        // check if s divide a previous sqrt arg
        gen S=recursive_normal(r2e(s,lv,contextptr),contextptr);
        if (S!=1){
          for (int i=0;i<larg.size();++i){
            if (lspow[i].type!=_SYMB)
              continue;
            gen g=_fxnd(larg[i]/S,contextptr);
            if (g.type==_VECT && g._VECTptr->size()==2 && g[1]==1){
              // replace lspow by pow(s,nover2)*pow(quo,nover2)
              gen in=lspow[i],expo=in._SYMBptr->feuille[1],quo=g[0];
              larg[i]=quo;
              lspow[i]=pow(quo,expo,contextptr);
              lout=subst(lout,in,pow(S,expo,contextptr)*lspow[i],false,contextptr);
            }
          }
        }
      }
      gen simpl,doubl; bool pos;
      zint2simpldoublpos(cont,simpl,doubl,pos,2,contextptr);
      if (!pos) simpl=-simpl;
      simpl=r2e(polynome(simpl,nd._POLYptr->dim),lv,contextptr); // if simpl is not an integer
      doubl=r2e(polynome(doubl,nd._POLYptr->dim),lv,contextptr); // if doubl is not an integer
      gen z=fast_icontent(simpl),zsimpl,zdoubl; bool zpos;
      zint2simpldoublpos(z,zsimpl,zdoubl,zpos,2,contextptr);
      if (zdoubl!=1){
	simpl=zsimpl*ratnormal(simpl/z);
	simplify3(den,zdoubl);
	doubl=zdoubl*doubl;
      }
      gen tmparg=r2e(den,lv,contextptr),tmpd=r2e(d,lv,contextptr);
      if (keep_abs){
	tmparg=abs(tmparg,contextptr);
	tmpd=abs(tmpd,contextptr);
      }
      gen prev=1,S=recursive_normal(r2e(s,lv,contextptr),contextptr);
      if (s.dim && !larg.empty()){
        // check if s is divisible by a previous sqrt arg
        for (int i=0;i<larg.size();++i){
          if (larg[i]==1) continue;
          gen g=_fxnd(S/larg[i],contextptr);
          if (g.type==_VECT && g._VECTptr->size()==2 && g[1]==1){
            prev=prev*pow(larg[i],nover2,contextptr);
            S=g[0];
          }
        }
      }
      larg.push_back(S);
      lspow.push_back(pow(S,nover2,contextptr));
      lout.push_back(prev*pow(simpl*out,nover2,contextptr)*pow(doubl,expnum,contextptr)*lspow.back()*pow(tmpd,expnum,contextptr)*pow(tmparg,-expnum,contextptr));
    }
    return subst(e,lin,lout,false,contextptr);
  }

  void cleanup_pow(gen & g){
    if (g.type==_VECT){
      vecteur & v=*g._VECTptr;
      for (int i=0;i<v.size();++i)
        cleanup_pow(v[i]);
    }
    if (g.is_symb_of_sommet(at_pow)){
      gen & f=g._SYMBptr->feuille;
      if (f.type==_VECT)
        f.subtype=_SEQ__VECT;
    }
  }

  gen normalize_sqrt(const gen & e,GIAC_CONTEXT,bool keep_abs){
    vecteur L;
    gen E=in_normalize_sqrt(rewrite_exp_integer(e,contextptr),L,keep_abs,contextptr);
    //cleanup_pow(E);
    return E;
  }

  static bool has_embedded_fractions(const gen & g){
    if (g.type==_POLY){
      vector< monomial<gen> >::const_iterator it=g._POLYptr->coord.begin(),itend=g._POLYptr->coord.end();
      for (;it!=itend;++it){
	if (has_embedded_fractions(it->value))
	  return true;
      }
      return false;
    }
    if (g.type==_VECT){
      const_iterateur it=g._VECTptr->begin(),itend=g._VECTptr->end();
      for (;it!=itend;++it){
	if (has_embedded_fractions(*it))
	  return true;
      }
      return false;
    }
    if (g.type==_FRAC){
      if (is_one(g._FRACptr->den))
	return has_embedded_fractions(g._FRACptr->num);
      return true;
    }
    return false;
  }

  static bool sort_func(const gen & a,const gen & b){
    if (a.type!=b.type)
      return a.type<b.type;
    if (a.type==_IDNT)
      return strcmp(a._IDNTptr->id_name,b._IDNTptr->id_name)<0;
    if (a.type==_SYMB){
      int cmp=strcmp(a._SYMBptr->sommet.ptr()->s,b._SYMBptr->sommet.ptr()->s);
      if (cmp) return cmp<0;
    }
    return a.print(context0)<b.print(context0);
  }
  static bool sort_func2(const gen & a,const gen & b){
    if (a.type!=b.type)
      return a.type<b.type;
    if (a.type==_IDNT)
      return strcmp(a._IDNTptr->id_name,b._IDNTptr->id_name)<0;
    if (a.type==_SYMB){
      int at=taille(a,256),bt=taille(b,256);
      if (at!=bt)
	return at<bt;
      if (a.is_symb_of_sommet(at_pow) && b.is_symb_of_sommet(at_pow)){
        const gen & af=a._SYMBptr->feuille;
        const gen & bf=b._SYMBptr->feuille;
        gen n1,d1,n2,d2;
        fxnd(af[1],n1,d1);
        fxnd(bf[1],n2,d2);
        if (d1!=d2)
          return is_greater(d1,d2,context0);
#if 1
        n1=af[0]; n2=bf[0];
        if (n1.type!=n2.type)
          return n1.type<n2.type;
        if (is_integer(n1)) return is_greater(n2,n1,context0);
#endif
      }
      int cmp=strcmp(a._SYMBptr->sommet.ptr()->s,b._SYMBptr->sommet.ptr()->s);
      if (cmp) return cmp<0;
    }
    return a.print(context0)<b.print(context0);
  }
  static vecteur sort1(const vecteur & l){
    vecteur res(l);
    gen_sort_f(res.begin(),res.end(),sort_func);
    return res;
  }
  static vecteur sort2(const vecteur & l){
    vecteur res(l);
    gen_sort_f(res.begin(),res.end(),sort_func2);
    return res;
  }
  static void sort0(vecteur & l){
    // changed 9 dec 2019 for b:=rootof([[-1,-2,-4*a+3,16*a],[1,0,8*a-6,-32*a+8,16*a^2+24*a-3]])/(32*a^2-8*a)*atan(x/sqrt(-(-1+sqrt(1-4*a))/2))-rootof([[1,-2,4*a-3,16*a],[1,0,8*a-6,32*a-8,16*a^2+24*a-3]])/(32*a^2-8*a)*atan(x/sqrt(-(-1-sqrt(1-4*a))/2));normal(diff(b));
    for (int i=1;i<l.size()-1;++i){
      if (l[i].type==_VECT && l[i]._VECTptr->empty()){
	l.erase(l.begin()+i);
	--i;
      }
    }
    iterateur it=l.begin(),itend=l.end();
    for (;it!=itend;++it){
      if (it->type==_VECT)
	*it=sort1(*it->_VECTptr);
    }
  }

  static bool do_mult_i(const gen & g){
    if (g.type==_CPLX && is_zero(*g._CPLXptr) && is_positive(*(g._CPLXptr+1),context0))
      return true;
    if (g.type==_POLY && do_mult_i(g._POLYptr->coord.front().value))
      return true;
    if (g.type!=_EXT)
      return false;
    if (g._EXTptr->type!=_VECT || g._EXTptr->_VECTptr->empty())
      return false;
    return do_mult_i(g._EXTptr->_VECTptr->front());
  }

  static void purgeassumelist(const vecteur & L,GIAC_CONTEXT){
    for (unsigned k=0;k<L.size();++k){
      gen Lk=ggb_var(L[k]);
      purgenoassume(Lk,contextptr);
    }
  }

  gen normal(const gen & e,bool distribute_div,bool allow_embeded_recursion,GIAC_CONTEXT){
    if (has_num_coeff(e))
      return ratnormal(e,contextptr);
    // COUT << e << "\n";
    if (e.type==_VECT){
      vecteur res;
      for (const_iterateur it=e._VECTptr->begin();it!=e._VECTptr->end();++it)
	res.push_back(normal(*it,distribute_div,contextptr));
      return gen(res,e.subtype);
    }
    if (e.type==_FRAC){
      gen n=e._FRACptr->num;
      gen d=e._FRACptr->den;
      simplify(n,d);
      if (is_one(d))
	return n;
      if (is_minus_one(d))
	return -n;
      if (is_zero(d)){
	if (is_zero(n))
	  return undef;
	else
	  return unsigned_inf;
      }
      if (is_zero(n))
	return zero;
      return fraction(n,d);
    }
    if (e.type==_MOD){
      gen p=*e._MODptr;
      gen m=*(e._MODptr+1);
      vecteur l(lvar(m));
      if (l.empty()){
	l=lvar(p);
	p=e2r(p,l,contextptr);
	gen num,den;
	fxnd(p,num,den);
	num=makemod(num,m);
	if (!is_one(den))
	  den=makemod(den,m);
	p=rdiv(num,den,contextptr);
	return r2e(p,l,contextptr);
      }
      return _quorem(makesequence(p,m),contextptr)[1];
    }
    if (e.type!=_SYMB)
      return e;
    if (is_equal(e)){
      vecteur & v=*e._SYMBptr->feuille._VECTptr;
      return symbolic(at_equal,makesequence(normal(v.front(),distribute_div,contextptr),normal(v.back(),distribute_div,contextptr)));
    }
    if (is_inf(e) || is_undef(e) )
      return e;
    gen ee,tmp;
    matrice l;
    fraction f(0);
    vecteur L;
#ifndef NO_STDEXCEPT
    try {
#endif
      ee=in_normalize_sqrt(e,L,true,contextptr);
      // put back integer content in sqrt, this may decrease the number
      // of extensions required, like for 
      // assume(0<a<1/4);b:=regroup(int(1/(x^4+x^2+a)));normal(diff(b));
      vecteur LL; gen EE=in_normalize_sqrt(inner_sqrt(e),LL,true,contextptr);
      if (lvar(EE).size()<lvar(ee).size()){
	ee=EE; L=LL;
      }
      ee=invfracpow(ee,contextptr);
      l=alg_lvar(ee);
      sort0(l);
      if (!L.empty() && debug_infolevel)
	*logptr(contextptr) << gettext("Making implicit assumption for sqrt argument ") << L << "\n";
      if (contextptr && contextptr->previous)
	L.clear(); // otherwise ggbsort(x):=sort(x); r:=[sqrt(a)/a,1]; ggbsort(r); VARS(); keeps implicit assumption a>=0 globally
      for (unsigned k=0;k<L.size();++k)
	giac_assume(L[k],contextptr);
      tmp=e2r(ee,l,contextptr);
      if (is_undef(tmp)){
	purgeassumelist(L,contextptr);
#ifdef GIAC_GGB
	return undef;
#endif
	if (calc_mode(contextptr)==1)
	  return undef;
	*logptr(contextptr) << gettext("Unable to build a single algebraic extension for simplifying.\nTrying rational simplification only. This might return a wrong answer if simplifying 0/0!") << "\n";
	l=lvar(ee);
	tmp=e2r(ee,l,contextptr);	
	gen tmpf=evalf_double(ee-tmp,1,contextptr);
	if (tmpf.type==_DOUBLE_ && is_greater(tmpf,epsilon(contextptr),contextptr))
	  return undef;
      }
#ifndef NO_STDEXCEPT
    }
    catch (std::runtime_error & err){
      last_evaled_argptr(contextptr)=NULL;
      CERR << err.what() << "\n";
      purgeassumelist(L,contextptr);
      return e;
    }
#endif
    if (tmp.type==_FRAC){
      f.num=tmp._FRACptr->num;
      f.den=tmp._FRACptr->den;
      if (f.num.type==_POLY && f.den.type==_POLY && lvar(ee)!=lidnt(l))
	simplify3(f.num,f.den);
    }
    else
      f=tmp;
    if (f.den.type==_CPLX){
      f.num=f.num*conj(f.den,contextptr);
      f.den=f.den*conj(f.den,contextptr);
    }
    if (do_mult_i(f.den)){
      f.num = -cst_i*f.num;
      f.den = -cst_i*f.den;
    }
    if (do_mult_i(-f.den)){
      f.num = cst_i*f.num;
      f.den = cst_i*f.den;
    }
    // search for embedded fractions
    if (has_embedded_fractions(f.num) || has_embedded_fractions(f.den)){
      gen res=r2sym(f,l,contextptr);
      purgeassumelist(L,contextptr);
      return allow_embeded_recursion?normal(res,distribute_div,false,contextptr):res;
    }
    if (distribute_div && f.num.type==_POLY && f.num._POLYptr->dim && f.den.type<_POLY){
      gen res=r2sym(gen(*f.num._POLYptr/f.den),l,contextptr);
      purgeassumelist(L,contextptr);
      return res;
    }
    if (!distribute_div){
      if (f.den.type==_POLY && is_positive(-f.den._POLYptr->coord.front())){
	f.num=-f.num;
	f.den=-f.den;
      }
      if (f.num.type==_POLY && !f.num._POLYptr->coord.empty() && is_positive(-f.num._POLYptr->coord.front())){
	f.num=-f.num;
	gen res=r2sym(f,l,contextptr);
	purgeassumelist(L,contextptr);	
	return symbolic(at_neg,res);
      }
    }
    ee=r2sym(f,l,contextptr);
    purgeassumelist(L,contextptr);
    if (!is_one(f.den) && is_integer(f.den)){
      ee=ratnormal(ratnormal(ee,contextptr),contextptr); // first ratnormal will expand sqrt()^
      // second will remove them
    }
    return ee; // ratnormal(ee,contextptr); // for sqrt(1-a^2)/sqrt(1-a)
  }

  gen normal(const gen & e,bool distribute_div,GIAC_CONTEXT){
    return normal(e,distribute_div,true,contextptr);
  }

  gen normal(const gen & e,GIAC_CONTEXT){
    return normal(e,true,contextptr);
  }

  // guess if g is a program/function: 1 func, 0 unknown, -1 not func
  static int is_program(const gen & g){
#ifdef GIAC_HAS_STO_38
    if (g.type==_IDNT){
      const char * idname=g._IDNTptr->id_name;
      if (strlen(idname)==2 && (idname[0]=='F' || idname[0]=='R' || idname[0]=='X' || idname[0]=='Y') && idname[1]>='0' && idname[1]<='9')
	return 1;
      else
	return -1;
    }
#endif
    if (g.type==_FUNC)
      return 1;
    if (g.type==_VECT){
      if (g.subtype==_PNT__VECT)
	return 0;
      const_iterateur it=g._VECTptr->begin(),itend=g._VECTptr->end();
      for (;it!=itend;++it){
	if (it->type==_STRNG)
	  return 0;
	int res=is_program(*it);
	if (res)
	  return res;
      }
      return 0;
    }
    if (g.type!=_SYMB)
      return 0;
    if (g.is_symb_of_sommet(at_of) && g._SYMBptr->feuille.type==_VECT){
      return is_program(g._SYMBptr->feuille._VECTptr->back());
    }
    if (g.is_symb_of_sommet(at_program))
      return 1;
    vecteur v(lvar(g));
    if (v.size()==1 && v.front()==g){
      if (g.type!=_SYMB)
	return 0;
      return is_program(g._SYMBptr->feuille);
    }
    for (unsigned i=0;i<v.size();++i){
      int res=is_program(v[i]);
      if (res)
	return res;
    }
    return 0;
  }

  bool guess_program(gen & g,GIAC_CONTEXT){
    if (is_program(g)!=1)
      return false;
    g=eval(g,1,contextptr);
    return true;
  }

  bool is_algebraic_program(const gen & g,gen & f1,gen & f3){
    if (g.type==_FUNC){
      if (g==at_equal || g==at_superieur_strict || g==at_superieur_egal || g==at_inferieur_strict || g==at_inferieur_egal)
	return false;
      identificateur _tmpi(" x");
      f1=_tmpi;    
      f3=g(f1,context0);
      return true;
    }
    if (!g.is_symb_of_sommet(at_program) || g._SYMBptr->feuille.type!=_VECT || g._SYMBptr->feuille._VECTptr->size()!=3){
      if (is_program(g)!=1)      
	return false;
      identificateur _tmpi(" x");
      f1=_tmpi;    
      f3=g(f1,context0);
      return true;      
    }
    f1=g._SYMBptr->feuille._VECTptr->front();
    if (f1.type==_VECT && f1.subtype==_SEQ__VECT && f1._VECTptr->size()==1)
      f1=f1._VECTptr->front();
    f3=g._SYMBptr->feuille._VECTptr->back();
    bool res= f3.type<=_IDNT || f3.type==_FRAC || f3.type==_VECT || (f3.type==_SYMB && !f3.is_symb_of_sommet(at_bloc));
    if (res)
      f3=eval(f3,1,context0); // eval operators like /
    return res;
  }
  bool has_algebraic_program(const gen & g){
    if (g.type==_VECT){
      vecteur & v=*g._VECTptr;
      for (unsigned i=0;i<v.size();++i){
	if (has_algebraic_program(v[i]))
	  return true;
      }
      return false;
    }
    if (g.type==_FUNC){
      if (g==at_equal || g==at_superieur_strict || g==at_superieur_egal || g==at_inferieur_strict || g==at_inferieur_egal)
	return false;
      return true;
    }
    if (!g.is_symb_of_sommet(at_program) || g._SYMBptr->feuille.type!=_VECT || g._SYMBptr->feuille._VECTptr->size()!=3){
      if (is_program(g)!=1)      
	return false;
      return true;      
    }
    gen f3=g._SYMBptr->feuille._VECTptr->back();
    bool res= ((f3.type==_SYMB || f3.type<=_IDNT) && !f3.is_symb_of_sommet(at_bloc));
    return res;
  }

  void recursive_lvar(vecteur & v){
    vecteur w(v);
    for (int i=0;i<v.size();++i){
      gen g=v[i];
      if (g.type!=_SYMB)
        continue;
      vecteur V=lvar(g._SYMBptr->feuille);
      recursive_lvar(V);
      for (int j=0;j<V.size();++j)
        if (!equalposcomp(w,V[j]))
          w.push_back(V[j]);
    }
    v=w;
  }

#if defined GIAC_HAS_STO_38 || defined FXCG || defined KHICAS
  bool algnum_normal(gen & e,GIAC_CONTEXT){
    return false;
  }

  bool algnum_rref(const matrice & a, matrice & res, vecteur & pivots, gen & det,int redtype,GIAC_CONTEXT){
    return false;
  }  
#else


  /* univariate algebraic extensions */
  gen rur_mult(const gen & a,const gen &b,const vecteur & pmin){
    gen r=a*b;
    if (r.type==_VECT){
      r=r%pmin;
      if (r.type==_VECT)
        r.subtype=_POLY1__VECT;
    }
    return r;
  }

  gen rur_prod(const gen & v,const vecteur & pmin){
    if (v.type!=_VECT)
      return v;
    if (v._VECTptr->empty())
      return 1;
    const vecteur & w=*v._VECTptr;
    int s=w.size();
    if (s==1)
      return v[0];
    if (s==2)
      return rur_mult(v[0],v[1],pmin);
    if (s==3)
      return rur_mult(rur_mult(v[0],v[1],pmin),v[2],pmin);
    vecteur v1(w.begin(),w.begin()+s/2),v2(w.begin()+s/2,w.end());
    gen n1=rur_prod(v1,pmin),n2=rur_prod(v2,pmin);
    return rur_mult(n1,n2,pmin);
  }

  gen rur_pow(const gen & n,int p,const vecteur & pmin){
    if (p==0) return 1;
    if (p==1) return n;
    gen a=rur_pow(n,p/2,pmin);
    a=rur_mult(a,a,pmin);
    if (p%2)
      return rur_mult(a,n,pmin);
    return a;
  }

  // is there any z1, z2 (!=0) such that z1*d1==z2*d2?
  bool same_deno(const gen & d1,const gen & d2,gen &z1,gen & z2){
    if (d1.type!=_VECT || d2.type!=_VECT)
      return false;
    const vecteur & v1=*d1._VECTptr; const vecteur & v2=*d2._VECTptr;
    int s=v1.size();
    if (s==0 || s!=v2.size())
      return false;
    z1=v2.front(); z2=v1.front();
    simplify3(z1,z2);
    for (int i=1;i<s;++i){
      if (z1*v1[i]!=z2*v2[i])
        return false;
    }
    return true;
  }
  
  bool rur_add(const gen & N,const gen & D,gen & n,gen & d,const vecteur & pmin){
    if (N.type!=_VECT || D.type!=_VECT)
      return true;
    if (N._VECTptr->empty() || D._VECTptr->empty()){
      n=0; d=1;
      return true;
    }
    const vecteur Nv=*N._VECTptr;
    const vecteur Dv=*D._VECTptr;
    int s=Nv.size();
    if (s!=Dv.size())
      return false;
    if (s==1){
      n=Nv[0];
      d=Dv[0];
      return true;
    }
    if (s==2){
      const gen & d0=Dv[0]; const gen & d1=Dv[1];
      if (d0==d1){
        n=Nv[0]+Nv[1];
        if (is_zero(n)){
          n=0; d=1; return true;
        }
        if (n.type==_VECT)
          n.subtype=_POLY1__VECT;
        d=d0;
        return true;
      }
      gen z0,z1;
      if (same_deno(d0,d1,z0,z1)){
        // Nv[0]/d0+Nv[1]/d1, d0*z0=d1*z1 -
        n=z0*Nv[0]+z1*Nv[1];
        if (is_zero(n)){
          n=0; d=1; return true;
        }
        d=z0*d0;
        return true;
      }
      n=Nv[0]*d1+Nv[1]*d0;
      modpoly rem,quo,tmp;
      if (n.type==_VECT){
        DivRem(*n._VECTptr,pmin,0,quo,rem);
        n=rem.empty()?0:rem;
        n.subtype=_POLY1__VECT;
      }
      if (is_zero(n)){
        n=0; d=1; return true;
      }
      if (d0.type==_VECT && d1.type==_VECT){
        tmp=operator_times(*d0._VECTptr,*d1._VECTptr,0);
        DivRem(tmp,pmin,0,quo,rem);
        d=rem;
      }
      else
        d=(d0*d1);//%pmin;
      if (d.type==_VECT)
        d.subtype=_POLY1__VECT;
      return true;
    }
    vecteur Nv1(Nv.begin(),Nv.begin()+s/2),Nv2(Nv.begin()+s/2,Nv.end());
    vecteur Dv1(Dv.begin(),Dv.begin()+s/2),Dv2(Dv.begin()+s/2,Dv.end());
    gen n1,n2,d1,d2;;
    if (!rur_add(Nv1,Dv1,n1,d1,pmin) || !rur_add(Nv2,Dv2,n2,d2,pmin))
      return false;
    if (d1==d2){
      n=n1+n2;
      if (n.type==_VECT)
        n.subtype=_POLY1__VECT;
      d=d1;
      return true;
    }
    gen z1,z2;
    if (same_deno(d1,d2,z1,z2)){
      // d1*z1=d2*z2, n1/d1+n2/d2=(n1*z1+n2*z2)/(d1*z1)
      n=z1*n1+z2*n2;
      if (is_zero(n)){
        n=0; d=1; return true;
      }
      d=z1*d1;
      return true;
    }
    modpoly rem,quo,tmp;
    n=n1*d2+n2*d1;
    if (n.type==_VECT){
      DivRem(*n._VECTptr,pmin,0,quo,rem);
      n=rem.empty()?0:rem;
      n.subtype=_POLY1__VECT;
    }
    if (is_zero(n)){
      n=0; d=1; return true;
    }
    if (d1.type==_VECT && d2.type==_VECT){
      tmp=operator_times(*d1._VECTptr,*d2._VECTptr,0);
      DivRem(tmp,pmin,0,quo,rem);
      d=rem;
    }
    else if (d1.type==_VECT || d2.type==_VECT)
      d=d1*d2 % pmin;
    else
      d=d1*d2;
    if (d.type==_VECT)
      d.subtype=_POLY1__VECT;
    return true;
  }
  
  bool rur_subst(const gen & g,const vecteur &vars,const vecteur & nums,const vecteur & dens,const vecteur & pmin,gen & n, gen & d,GIAC_CONTEXT){
    if (0 && g.type==_CPLX){
      gen r,i;
      reim(g,r,i,contextptr);
      n=r+i*nums[nums.size()-1];
      d=dens[nums.size()-1];
      return true;
    }
    if (g.type==_IDNT){
      int i=equalposcomp(vars,g);
      if (!i){
        return false;
        // or
        n=g;
        d=1;
        return true;
      }
      n=nums[i-1];
      if (n.type==_VECT) n.subtype=_POLY1__VECT;
      d=dens[i-1]; d.subtype=_POLY1__VECT;
      return true;
    }
    if (g.type==_FRAC){
      n=g._FRACptr->num;
      d=g._FRACptr->den;
      return true;
    }
    if (g.type==_VECT){
      const vecteur & v=*g._VECTptr;
      vecteur N,D;
      for (int i=0;i<v.size();++i){
        if (!rur_subst(v[i],vars,nums,dens,pmin,n,d,contextptr))
          return false;
        N.push_back(n);
        D.push_back(d);
      }
      n=N; d=D;
      return true;
    }
    if (g.type==_SYMB){
      if (g._SYMBptr->sommet==at_pow){
        gen base=g._SYMBptr->feuille[0],expo=g._SYMBptr->feuille[1];
        if (expo.type!=_INT_ || !rur_subst(base,vars,nums,dens,pmin,n,d,contextptr))
          return false;
        int p=expo.val;
        if (p<0){
          swapgen(n,d);
          p=-p;
        }
        if (p==0) return false;
        if (p==1) return true;
        n=rur_pow(n,p,pmin);
        d=rur_pow(d,p,pmin);
        return true;
      }
      if (!rur_subst(g._SYMBptr->feuille,vars,nums,dens,pmin,n,d,contextptr))
        return false;
      if (g._SYMBptr->sommet==at_inv){
        swapgen(n,d);
        return true;
      }
      if (g._SYMBptr->sommet==at_neg){
        n=-n;
        return true;
      }
      if (g._SYMBptr->sommet==at_prod){
        n=rur_prod(n,pmin);
        d=rur_prod(d,pmin);
        return true;
      }
      if (g._SYMBptr->sommet==at_plus){
        rur_add(n,d,n,d,pmin);
        return true;
      }
    }
    n=g;
    d=1;
    return true;
  }

  gen rur_subst(const gen &E,const vecteur &vars,const vecteur & nums_,const gen & den_,const vecteur & pmin,GIAC_CONTEXT){
    gen n,d;
    vecteur lv(lidnt(den_));
    if (lv.empty())
      lv=lidnt(nums_);
    gen x=lv.front();
    vecteur nums(nums_),dens(nums.size());
    gen den=_symb2poly(makesequence(den_,x),contextptr);
    for (int i=0;i<nums.size();++i){
      n=_symb2poly(makesequence(nums[i],x),contextptr);
      if (n.type==_VECT){
        vecteur v=*n._VECTptr;
        lcmdeno(v,d,contextptr);
        nums[i]=gen(v,_POLY1__VECT);
      }
      else 
        fxnd(n,nums[i],d);
      dens[i]=d*den;
    }
    if (!rur_subst(E,vars,nums,dens,pmin,n,d,contextptr))
      return undef;
    if (is_exactly_zero(n))
      return 0;
    if (0){
      // invert d with bezout
      gen u,v,g;
      egcd(d,pmin,u,v,g); // u*d=g mod pmin => n/d=(n*u)/g
      rur_mult(n,u,pmin);
      d=g;
    }
    if (n.type==_VECT) n.subtype=_POLY1__VECT;
    if (d.type==_VECT) d.subtype=_POLY1__VECT;
    return makevecteur(n,d); 
  }

  /* multivariate algebraic extensions */
  gen r2e_recursive(const polynome & p,const vecteur & l,GIAC_CONTEXT){
    if (l.empty() || p.dim==0)
      return r2e(p,l,contextptr);
    vecteur v;
    polynome2poly1(p,1,v);
    gen x=l[0]; vecteur ltail(l.begin()+1,l.end());
    for (int i=0;i<v.size();++i){
      if (v[i].type==_POLY)
        v[i]=r2e_recursive(*v[i]._POLYptr,ltail,contextptr);
    }
    return horner(v,x);
  }

  void mreduce(polynome & r,polynome & d,int s,const vector<polynome> & systnum,const  vector< vector<int> > & transpositions,GIAC_CONTEXT){
    int dim=r.dim;
    d.dim=r.dim;
    d.coord.clear();
    d.coord.push_back(monomial<gen>(1,dim));
    if (s<1 || r.degree(s-1)<systnum[s-1].degree(s-1))
      return;
    polynome quo(dim),rem(dim),a(dim);
    for (int j=s-1;j>=0;--j){
      if (j)
        r.reorder(transpositions[j]);
      r.TPseudoDivRem(systnum[j],quo,rem,a);
      if (j){
        a.reorder(transpositions[j]);
        rem.reorder(transpositions[j]);
      }
      r.coord.swap(rem.coord);
      d=a*d;
      if (j){
        polynome dd(dim);
        mreduce(d,dd,j,systnum,transpositions,contextptr);
        r=r*dd;
      }
    }
  }

  void mreduce(polynome & r,polynome & d,const vector<polynome> & systnums,const  vector< vector<int> > & transpositions,GIAC_CONTEXT){
    int s=systnums.size();
    mreduce(r,d,s,systnums,transpositions,contextptr);
  }
  
  int algnum_gcd(polynome & a,polynome & b,vecteur & syst,int s,const vecteur & lv,vector<polynome> & systnums,const vecteur & varsymb,vecteur & varapprox,const vector< vector<int> > & transpositions,ext_param_t & extpar,GIAC_CONTEXT);

  gen algnum_evalf(const gen & g,int digits,GIAC_CONTEXT){
#ifdef HAVE_LIBMPFI
    return convert_interval(g,digits*10/3,contextptr);
#else
    return _evalf(makesequence(g,digits),contextptr);
#endif
  }

  void algnum_increase_precision(const vecteur & varsymb,vecteur & varapprox,ext_param_t & extpar,GIAC_CONTEXT){
    if (extpar.digits==extpar.max_digits)
      return;
    int digits=2*extpar.digits;
    if (digits>extpar.max_digits)
      digits=extpar.max_digits;
    extpar.digits=digits;
    varapprox=gen2vecteur(algnum_evalf(varsymb,digits,contextptr));
  }
  
  // check if a is 0, if so modifies syst
  bool algnum_is_zero(const polynome & a,vecteur & syst,int s,const vecteur & lv,vector<polynome> & systnums,const vecteur & varsymb,vecteur & varapprox,const vector< vector<int> > & transpositions,ext_param_t & extpar,GIAC_CONTEXT){
    if (s==0)
      return false;
    // first try with num eval
    gen r=peval(a,varapprox,0);
    if (!is_zero_or_contains(r,contextptr)
        // is_greater(abs(r,contextptr),1e-6,contextptr)
        )
      return false;
    // r value is small, try to prove a smaller degree relation in syst
    polynome g(a),h(systnums[s-1]);
    if (s>=2)
      h.reorder(transpositions[s-1]); // since systnums[s-1] is already ready for division and therefore transpositionned
    if (algnum_gcd(g,h,syst,s,lv,systnums,varsymb,varapprox,transpositions,extpar,contextptr)==0){
      if (s>=2){
        // recursively check coefficients of a
        g=a;
        g.reorder(transpositions[s-1]);
        vector< monomial<gen> >::const_iterator it=g.coord.begin(),itend=g.coord.end();
        while (it!=itend){
          polynome lc(Tnextcoeff<gen>(it,itend));
          lc=lc.untrunc1(0);
          lc.reorder(transpositions[s-1]);
          if (!algnum_is_zero(lc,syst,s-1,lv,systnums,varsymb,varapprox,transpositions,extpar,contextptr)){
            algnum_increase_precision(varsymb,varapprox,extpar,contextptr);
            return false;
          }
        }
        return true;
      }
      algnum_increase_precision(varsymb,varapprox,extpar,contextptr);
      return false; 
    }
    // g is the gcd, if it evals to 0 then replace in syst and systnums
    r=peval(g,varapprox,0);
    if (!is_zero(r,contextptr)
        //is_greater(abs(r,contextptr),1e-6,contextptr)
        ){
      // replace g by cofactor of g in syst[s-1]
      polynome A(systnums[s-1]),quo(a.dim),rem(a.dim),c(a.dim);
      if (s>=2)
        g.reorder(transpositions[s-1]);
      A.TPseudoDivRem(g,quo,rem,c);
      g.coord.swap(quo.coord);
      if (s>=2)
        g.reorder(transpositions[s-1]);
    }
    syst[s-1]=r2e(g,lv,contextptr);
    systnums[s-1]=g;
    return true;
  }

  bool algnum_rref_is_zero(const gen & a,vecteur & syst,const vecteur & lv,vector<polynome> & systnums,const vecteur & varsymb,vecteur & varapprox,const vector< vector<int> > & transpositions,ext_param_t & extpar,GIAC_CONTEXT){
    if (!extpar.chkext || a.type!=_POLY)
      return is_zero(a,contextptr);
    return algnum_is_zero(*a._POLYptr,syst,a._POLYptr->dim,lv,systnums,varsymb,varapprox,transpositions,extpar,contextptr);
  }
  
  // replace a by gcd of a and b with respect to variable s-1, where variables
  // 0 to s-2 satisfy systnums
  // returns gcd degree
  // may modify syst and systnums
  // syst is the symbolic system with respect to lv variables
  // lv values are in varsymb (exact symbolic) and varapprox (num eval current precision)
  // for example assumes x1^2-2=0, x2^2-3=0, x3^2-6=0
  // Is x3-x1*x2==0?
  // Computing gcd(x3^2-6,x3-x1*x2) with algnum_gcd will replace
  // a by x3+x1*x2 because the remainder of x3^2-6 by x3-x1*x2 is x1^2*x2^2-6
  // and this reduces to 0
  // This information may be used to simplify syst
  int algnum_gcd(polynome & a,polynome & b,vecteur & syst,int s,const vecteur & lv,vector<polynome> & systnums,const vecteur & varsymb,vecteur & varapprox,const vector< vector<int> > & transpositions,ext_param_t & extpar,GIAC_CONTEXT){
    if (a.degree(s-1)<b.degree(s-1))
      a.coord.swap(b.coord);
    int dim=a.dim;
    polynome quo(dim),rem(dim),c(dim);
    while (!b.coord.empty()){
      int n=b.degree(s-1);
      if (n==0){
        a.coord.swap(b.coord);
        b.coord.clear();
        break;
      }
      if (s>=2){
        b.reorder(transpositions[s-1]);
        a.reorder(transpositions[s-1]);
      }
      a.TPseudoDivRem(b,quo,rem,c);
      a.coord.swap(b.coord);
      b.coord.swap(rem.coord);
      if (s>=2){
        a.reorder(transpositions[s-1]);
        b.reorder(transpositions[s-1]);
      }
      // now reduce b
      for (int j=s-2;j>=0;--j){
        if (j)
          b.reorder(transpositions[j]);
        b.TPseudoDivRem(systnums[j],quo,rem,c);
        if (j)
          rem.reorder(transpositions[j]);
        b.coord.swap(rem.coord);
      }
      // trim: lcoeff of b must be !=0
      if (s>=2) b.reorder(transpositions[s-1]);
      vector< monomial<gen> >::const_iterator it=b.coord.begin(),itend=b.coord.end();
      while (it!=itend){
        polynome lc(Tnextcoeff<gen>(it,itend));
        lc=lc.untrunc1(0);
        if (s-1) lc.reorder(transpositions[s-1]);
        if (!algnum_is_zero(lc,syst,s-1,lv,systnums,varsymb,varapprox,transpositions,extpar,contextptr))
          break;
        b.coord.erase(b.coord.begin(),b.coord.begin()+(it-b.coord.begin())); // second argument rewritten, was rem.coord.erase(rem.coord.begin(),it);
        it=b.coord.begin(); itend=b.coord.end();
      }
      if (s>=2) b.reorder(transpositions[s-1]);
    }
    return a.degree(s-1);
  }
  
  gen mreduce_back(polynome & r,const vecteur & lv,GIAC_CONTEXT){
    if (r.coord.empty())
      return 0;    
    index_t idx=r.gcddeg(); polynome a(monomial<gen>(plus_one,idx));;
    r=r.shift(-idx);
    gen R=r2e_recursive(a,lv,contextptr)*r2e_recursive(r,lv,contextptr);
    // gen R=r2e_recursive(r,lv,contextptr)/r2e(d,lv,contextptr);
    return R;
  }
  gen mreduce_back(const gen & r,const vecteur & lv,GIAC_CONTEXT){
    gen n,d; fxnd(r,n,d);
    if (n.type==_POLY)
      n=mreduce_back(*n._POLYptr,lv,contextptr);
    if (d.type==_POLY)
      d=mreduce_back(*d._POLYptr,lv,contextptr);
    return n/d;
  }
  gen algnum_reduce(polynome & r,vecteur & syst,const vecteur & lv,vector<polynome> & systnums,const vecteur & varsymb,vecteur & varapprox,const vector< vector<int> > & transpositions,ext_param_t & extpar,GIAC_CONTEXT){
    polynome d(r.dim);
    if (extpar.chkext){
      polynome r_orig(r);
      for (;;){
        r=r_orig;
        mreduce(r,d,systnums,transpositions,contextptr);
        if (r.coord.empty())
          break;
        if (!algnum_is_zero(d,syst,syst.size(),lv,systnums,varsymb,varapprox,transpositions,extpar,contextptr)){
          if (algnum_is_zero(r,syst,syst.size(),lv,systnums,varsymb,varapprox,transpositions,extpar,contextptr))
            r.coord.clear();
          break;
        }
      }
    }
    else
      mreduce(r,d,systnums,transpositions,contextptr);
    return mreduce_back(r,lv,contextptr)/mreduce_back(d,lv,contextptr);;
  }

  void lazy_reduce(gen & N,gen & D,const vector<polynome> & systnums,const  vector< vector<int> > & transpositions,GIAC_CONTEXT){
    for (;;){
      if (N.type==_POLY){
        polynome n(*N._POLYptr),d(n.dim);
        mreduce(n,d,n.dim,systnums,transpositions,contextptr);
        N=n;
        gen dg(d);
        D=dg*D;
      }
      if (D.type!=_POLY)
        break;
      polynome n(*D._POLYptr),d(n.dim);
      mreduce(n,d,n.dim,systnums,transpositions,contextptr);
      D=n;
      gen dg(d);
      N=N*dg;
      if (dg.type!=_POLY)
        break;
    }
  }

  void lazy_dbg(const gen & x,const gen & N,const gen & D,const vecteur & vars,const vecteur & varapprox,GIAC_CONTEXT){
    if (0){
      if (x.type==_VECT){
        for (int i=0;i<x._VECTptr->size();++i)
          lazy_dbg(x[i],N[i],D[i],vars,varapprox,contextptr);
        return;
      }
      gen xx=subst(x,vars,varapprox,false,contextptr);
      gen n=N.type==_POLY?peval(*N._POLYptr,varapprox,0):N;
      gen d=D.type==_POLY?peval(*D._POLYptr,varapprox,0):D;
      gen delta=abs(xx-n/d,contextptr);
      if (is_greater(delta,1e-6,contextptr))
        cout << x << " " << delta << endl;
    }
  }    
  
  void lazy_dbgreduce(const gen & n,const gen & d,const gen & N,const gen & D,const vecteur & vars,const vecteur & varapprox,GIAC_CONTEXT){
    if (0){
      gen NN=N.type==_POLY?peval(*N._POLYptr,varapprox,0):N;
      gen DD=D.type==_POLY?peval(*D._POLYptr,varapprox,0):D;
      gen nn=n.type==_POLY?peval(*n._POLYptr,varapprox,0):n;
      gen dd=d.type==_POLY?peval(*d._POLYptr,varapprox,0):d;
      gen delta=abs(nn/dd-NN/DD,contextptr);
      if (is_greater(1e-6,abs(DD,contextptr),contextptr) || is_greater(1e-6,abs(NN,contextptr),contextptr) || is_greater(delta,1e-6,contextptr))
        cout << "lazy_dbgreduce " << delta << " " << D << endl;
    }
  }    
  
  void lazy_dbg(const gen & x,int beg,int end,const gen & N,const gen & D,const vecteur & vars,const vecteur & varapprox,GIAC_CONTEXT){
    if (0 && x.type==_SYMB){
      gen x1(symbolic(x._SYMBptr->sommet,gen(vecteur(x._SYMBptr->feuille._VECTptr->begin()+beg,x._SYMBptr->feuille._VECTptr->begin()+end),x._SYMBptr->feuille.subtype)));
      lazy_dbg(x1,N,D,vars,varapprox,contextptr);
    }
  }    
  
  void lazy_dbgsum(const gen & n,const gen & d,const gen & N,const gen & D,const vecteur & vars,const vecteur & varapprox,GIAC_CONTEXT){
    if (0 && n.type==_VECT){
      gen xx=0;
      vecteur & v=*n._VECTptr; vecteur & w=*d._VECTptr;
      for (int i=0;i<v.size();++i){
        xx += (v[i].type==_POLY?peval(*v[i]._POLYptr,varapprox,0):v[i])/(w[i].type==_POLY?peval(*w[i]._POLYptr,varapprox,0):w[i]);
      }
      gen n_=N.type==_POLY?peval(*N._POLYptr,varapprox,0):N;
      gen d_=D.type==_POLY?peval(*D._POLYptr,varapprox,0):D;
      gen delta=abs(xx-n_/d_,contextptr);
      if (is_greater(delta,1e-6,contextptr))
        cout << "lazy_dbgsum err " << delta << endl;
    }
  }    
  
  int lazy_prod(const gen & feuille,gen & N,gen & D,const vector<polynome> & systnums,const  vector< vector<int> > & transpositions,GIAC_CONTEXT){
    D=1;
    int s=systnums.size();
    if (feuille.type!=_VECT){
      N=feuille;
      return 1;
    }
    const vecteur & v =*feuille._VECTptr;
    if (v.empty()){ N=0; return 0;}
    if (v.size()==1){ N=v[0]; return 1; }
    if (v.size()==2){
      N=v[0]*v[1];
      if (N.type==_POLY){
        polynome n(*N._POLYptr),d(systnums.size());
        mreduce(n,d,s,systnums,transpositions,contextptr);
        N=n; D=d;
      }
      return 2;
    }
    vecteur v1(v.begin(),v.begin()+v.size()/2);
    vecteur v2(v.begin()+v.size()/2,v.end());
    gen n1,d1,n2,d2;
    lazy_prod(v1,n1,d1,systnums,transpositions,contextptr);
    lazy_prod(v2,n2,d2,systnums,transpositions,contextptr);
    N=n1*n2;
    D=d1*d2;
    lazy_reduce(N,D,systnums,transpositions,contextptr);
    return v.size();
  }

  int lazy_sum(const vecteur & nums,const vecteur & dens,gen & N,gen & D,const vector<polynome> & systnums,const vecteur & vars,vecteur & varapprox,const  vector< vector<int> > & transpositions,GIAC_CONTEXT){
    D=1;
    int s=systnums.size();
    if (nums.empty()){ N=0; return 0;}
    if (nums.size()==1){ N=nums[0]; D=dens[0]; return 1; }
    if (nums.size()==2){
      if (dens[0]==dens[1]){
        N=nums[0]+nums[1];
        D=dens[0];
        return 2;
      }
      N=nums[0]*dens[1]+nums[1]*dens[0];
      if (N.type==_POLY){
        polynome n(*N._POLYptr),d(systnums.size());
        mreduce(n,d,s,systnums,transpositions,contextptr);
        lazy_dbgreduce(N,D,n,d,vars,varapprox,contextptr);
        N=n; D=d;
      }
      D=D*dens[0]*dens[1];
      if (D.type==_POLY){
        polynome n(*D._POLYptr),d(systnums.size());
        mreduce(n,d,s,systnums,transpositions,contextptr);
        lazy_dbgreduce(D,1,n,d,vars,varapprox,contextptr);
        N=gen(d)*N; D=n;
      }
      lazy_dbgsum(nums,dens,N,D,vars,varapprox,contextptr);
      return 2;
    }
    vecteur num1(nums.begin(),nums.begin()+nums.size()/2);
    vecteur num2(nums.begin()+nums.size()/2,nums.end());
    vecteur den1(dens.begin(),dens.begin()+dens.size()/2);
    vecteur den2(dens.begin()+dens.size()/2,dens.end());
    gen n1,d1,n2,d2;
    lazy_sum(num1,den1,n1,d1,systnums,vars,varapprox,transpositions,contextptr);
    lazy_dbgsum(num1,den1,n1,d1,vars,varapprox,contextptr);
    lazy_sum(num2,den2,n2,d2,systnums,vars,varapprox,transpositions,contextptr);
    lazy_dbgsum(num2,den2,n2,d2,vars,varapprox,contextptr);
    vecteur nn(makevecteur(n1,n2)),dd(makevecteur(d1,d2));
    lazy_sum(nn,dd,N,D,systnums,vars,varapprox,transpositions,contextptr);
    lazy_dbgsum(nn,dd,N,D,vars,varapprox,contextptr);
    return nums.size();
  }

  int lazy_pow(const gen & base,int n,gen & N,gen & D,const vector<polynome> & systnums,const  vector< vector<int> > & transpositions,GIAC_CONTEXT){
    if (n==0){
      N=1; D=1; return 0;
    }
    if (n==1){
      N=base; D=1; return 1;
    }
    lazy_pow(base,n/2,N,D,systnums,transpositions,contextptr);
    N=N*N;
    D=D*D;
    if (n%2)
      N=base*N;
    lazy_reduce(N,D,systnums,transpositions,contextptr);
    return n;
  }

  // returns -1 on error, 0 on undef, 1 otherwise
  int algnum_lazy_convert(const gen & x,vecteur & syst,const vecteur & vars,vector<polynome> & systnums,const vecteur & varsymb,vecteur & varapprox,vector< vector<int> > & transpositions,ext_param_t & extpar,vecteur &lv,gen & N,gen & D,GIAC_CONTEXT){
    int s=syst.size();
    if (x.type==_FRAC){
      N=x._FRACptr->num;
      D=x._FRACptr->den;
      return true;
    }
    if (x.type==_VECT){
      const vecteur & v =*x._VECTptr;
      vecteur Nv(v.size()),Dv(v.size());
      for (int i=0;i<v.size();++i){
        int res=algnum_lazy_convert(v[i],syst,vars,systnums,varsymb,varapprox,transpositions,extpar,lv,N,D,contextptr);
        if (res<=0)
          return res;
        Nv[i]=N;
        Dv[i]=D;
      }
      N=Nv;
      D=Dv;
      return 1;
    }
    if (x.type==_IDNT || x.type==_SYMB){
      int i=equalposcomp(vars,x);
      if (i){
        polynome n(vars.size());
        n.coord.push_back(monomial<gen>(1,1,i,vars.size()));
        N=n;
        D=1;
        return 1;
      }
    }
    if (x.type==_SYMB){
      if (x._SYMBptr->sommet==at_pow){
        gen & f=x._SYMBptr->feuille;
        if (f.type==_VECT && f._VECTptr->size()==2 && f._VECTptr->back().type==_INT_){
          int n=f._VECTptr->back().val;
          int res=algnum_lazy_convert(f._VECTptr->front(),syst,vars,systnums,varsymb,varapprox,transpositions,extpar,lv,N,D,contextptr);
          if (res<=0)
            return res;
          if (n<0){
            if (extpar.chkext && N.type==_POLY && algnum_is_zero(*N._POLYptr,syst,syst.size(),vars,systnums,varsymb,varapprox,transpositions,extpar,contextptr))
              return 0;
            swapgen(N,D);
            n=-n;
          }
          gen n1,d1,n2,d2;
          lazy_pow(N,n,n1,d1,systnums,transpositions,contextptr);
          lazy_pow(D,n,n2,d2,systnums,transpositions,contextptr);
          N=n1*d2;
          D=n2*d1;
          lazy_reduce(N,D,systnums,transpositions,contextptr);
          lazy_dbg(x,N,D,vars,varapprox,contextptr);
          return 1;
        }
      }
      int res=algnum_lazy_convert(x._SYMBptr->feuille,syst,vars,systnums,varsymb,varapprox,transpositions,extpar,lv,N,D,contextptr);
      lazy_dbg(x._SYMBptr->feuille,N,D,vars,varapprox,contextptr);
      if (res<=0)
        return res;
      if (x._SYMBptr->sommet==at_neg){
        N=-N;
        return 1;
      }
      if (x._SYMBptr->sommet==at_prod){
        gen n1,d1,n2,d2;
        lazy_prod(N,n1,d1,systnums,transpositions,contextptr);
        lazy_prod(D,n2,d2,systnums,transpositions,contextptr);
        N=n1*d2; D=n2*d1;
        lazy_reduce(N,D,systnums,transpositions,contextptr);
        lazy_dbg(x,N,D,vars,varapprox,contextptr);
        return 1;
      }
      if (x._SYMBptr->sommet==at_inv){
        if (extpar.chkext && N.type==_POLY && algnum_is_zero(*N._POLYptr,syst,syst.size(),vars,systnums,varsymb,varapprox,transpositions,extpar,contextptr))
          return 0;
        swapgen(N,D);
        return 1;
      }
      if (x._SYMBptr->sommet==at_plus){
        if (N.type!=_VECT || D.type!=_VECT)
          return 1;
        gen n,d;
        lazy_sum(*N._VECTptr,*D._VECTptr,n,d,systnums,vars,varapprox,transpositions,contextptr);
        lazy_dbg(x,n,d,vars,varapprox,contextptr);
        N=n;
        D=d;
        return 1;
      }
    }
    N=x;
    D=1;
    return 2;
  }

  // returns 0 if syst not polynomial, -1 if x not polynomial, 1 otherwise
  int prepare(const gen & x,vecteur & syst,const vecteur & vars,vector<polynome> & systnums,const vecteur & varsymb,vecteur & varapprox,vector< vector<int> > & transpositions,ext_param_t & extpar,vecteur &lv,polynome & n,gen & D,GIAC_CONTEXT){
    lv=vars;
    lvar(x,lv);
    lvar(syst,lv);
    int dim=-1,s=syst.size();
    systnums.resize(s);
    transpositions.resize(s);
    for (int j=0;j<s;++j){
      gen tmp=e2r(syst[j],lv,contextptr),tmpn,tmpd;
      fxnd(tmp,tmpn,tmpd);
      if (tmpn.type!=_POLY)
        return 0;
      systnums[j]=*tmpn._POLYptr;
      dim=systnums[j].dim;
      if (j){
        transpositions[j]=transposition(0,j,dim);
        systnums[j].reorder(transpositions[j]);
      }
    }
    // e2r call should be replaced using reduction by syst
    gen N; int res=-1;
    if (1)
      res=algnum_lazy_convert(x,syst,vars,systnums,varsymb,varapprox,transpositions,extpar,lv,N,D,contextptr);
    if (res<=0){
      gen X=e2r(x,lv,contextptr);
      fxnd(X,N,D);
    }
    if (N.type==_POLY)
      n = *N._POLYptr;
    else if (dim>=0 && is_exactly_zero(N))
      n= polynome(dim);
    else
      return -1;
    return 1;
  }

  gen mreduce_gen(const gen & x,vecteur & syst,const vecteur & vars,const vecteur & v,vecteur & varapprox,ext_param_t & extpar,GIAC_CONTEXT){
    vector<polynome> systnums; vecteur lv; vector< vector<int> > transpositions; polynome n; gen D;
    if (prepare(x,syst,vars,systnums,v,varapprox,transpositions,extpar,lv,n,D,contextptr)<=0)
      return x;
    // now reduce
    if (D.type==_POLY)
      D=algnum_reduce(*D._POLYptr,syst,lv,systnums,v,varapprox,transpositions,extpar,contextptr);
    gen N=algnum_reduce(n,syst,lv,systnums,v,varapprox,transpositions,extpar,contextptr);
    return N/D;
  }

  gen algnum_reduce_rref(const gen & x,vecteur & syst,const vecteur & lv,vector<polynome> & systnums,const vecteur & varsymb,vecteur & varapprox,const vector< vector<int> > & transpositions,ext_param_t & extpar, GIAC_CONTEXT){
    gen N,D;
    fxnd(x,N,D);
    if (N.type!=_POLY && D.type!=_POLY)
      return x;
    gen ND=1,DD=1;
    if (N.type==_POLY){
      polynome nd;
      mreduce(*N._POLYptr,nd,systnums,transpositions,contextptr);
      ND=nd;
    }
    if (D.type==_POLY){
      polynome dd;
      mreduce(*D._POLYptr,dd,systnums,transpositions,contextptr);
      DD=dd;
    }
    return (N*DD)/(D*ND);
  }

  gen horner_rur(const vecteur & p,const gen & x,const vecteur & syst,const vecteur & vars,GIAC_CONTEXT,const gen & zd=1){
    int s=giacmin(syst.size(),vars.size());
    if (1){
      // this should be faster once back conversion at the end is returned in recursive form instead of distributed
      vecteur lv(vars);
      lvar(x,lv);
      lvar(syst,lv);
      gen X=e2r(x,lv,contextptr),N,D;
      fxnd(X,N,D);
      if (D.type==_POLY || N.type!=_POLY)
        return horner(p,x);
      polynome & n =*N._POLYptr;
      int dim=n.dim;
      polynome r(dim); polynome d(dim);
      r.coord.push_back(monomial<gen>(p[0],dim));
      d.coord.push_back(monomial<gen>(1,dim));
      vector<polynome> systnums(s);
      vector< vector<int> > transpositions(s);
      for (int j=0;j<s;++j){
        gen tmp=e2r(syst[j],lv,contextptr),tmpn,tmpd;
        fxnd(tmp,tmpn,tmpd);
        if (tmpn.type!=_POLY)
          return horner(p,x)/zd;
        systnums[j]=*tmpn._POLYptr;
        if (j){
          transpositions[j]=transposition(0,j,dim);
          systnums[j].reorder(transpositions[j]);
        }
      }
      polynome quo,rem,a;
      for (int i=1;i<p.size();++i){
        // r/d*n/D+p[i]= (r*n+p[i]*d*D)/(d*D)
        d=D*d;
        r=r*n;
        a=p[i]*d;
        r=r+a;
        // now reduce r/d
        for (int j=s-1;j>=0;--j){
          if (j)
            r.reorder(transpositions[j]);
          r.TPseudoDivRem(systnums[j],quo,rem,a);
          if (j){
            a.reorder(transpositions[j]);
            rem.reorder(transpositions[j]);
          }
          r.coord.swap(rem.coord);
          d=a*d;
        }
      }
      index_t idx=r.gcddeg(); a=polynome(monomial<gen>(plus_one,idx));;
      r=r.shift(-idx);
      gen zn=ppz(r);
      gen zzd=zd*ppz(d);
      simplify3(zn,zzd);
      if (zzd.type==_CPLX){
        gen zcd=conj(zzd,contextptr);
        zn=zn*zcd;
        zzd=zzd*zcd;
      }
      gen R=zn*r2e_recursive(a,lv,contextptr)*r2e_recursive(r,lv,contextptr)/r2e(d,lv,contextptr)/zzd;
      // gen R=r2e_recursive(r,lv,contextptr)/r2e(d,lv,contextptr);
      return R;
    }
    gen rR=0;
    for (int i=0;i<p.size();++i){
      // rR=rR*x+p[i] modulo syst
      rR=rR*x+p[i];
      for (int j=s-1;j>=0;--j){
        rR=_rem(makesequence(rR,syst[j],vars[j]),contextptr);
      }
    }
    return rR;
  }
  
  void lrdm(modpoly & p,int n);
  bool algnum_rewritable(const gen & E_,vecteur & syst,const vecteur &vars,const vecteur & v,vecteur & varapprox,const vecteur & nums,const gen & diffpmin,const vecteur & pminv,const gen & pmin,const gen & var, const gen& ext,const gen & extapprox,const gen & sep,bool rootofallowed,bool ckdeg2,ext_param_t & extpar,gen & e,GIAC_CONTEXT){
    // DOIT should extract denominator of nums?
    if (E_.type==_VECT){
      int s=E_._VECTptr->size();
      const vecteur & Ev=*E_._VECTptr;
      vecteur ans(Ev);
      for (int i=0;i<s;++i){
        gen ee;
        if (!algnum_rewritable(Ev[i],syst,vars,v,varapprox,nums,diffpmin,pminv,pmin,var,ext,extapprox,sep,rootofallowed,ckdeg2,extpar,ee,contextptr))
          return false;
        ans[i]=ee;
      }
      e=gen(ans,E_.subtype);
      return true;
    }
    bool failed=true;
    gen E(E_),En,Ed;
    if (1){
      // DOIT should reduce E wrt syst?
      gen EE=mreduce_gen(E,syst,vars,v,varapprox,extpar,contextptr);
      if (extpar.chkext){
        e=subst(EE,vars,v,false,contextptr);
        return true;
      }
      EE=rur_subst(EE,vars,nums,diffpmin,pminv,contextptr);
      if (EE==0){
        e=0;
        return true;
      }
      failed=is_undef(EE);
      if (!failed){
        if (EE.type!=_VECT)
          return false;
        En=EE[0]; Ed=EE[1];
        if (ckdeg2 && Ed.type==_VECT){
          // detect if E is solution of a 2nd order equation
          gen EE2=mreduce_gen(pow(E,2,contextptr),syst,vars,v,varapprox,extpar,contextptr);
          EE2=rur_subst(EE2,vars,nums,diffpmin,pminv,contextptr);
          if (EE2.type==_VECT && EE2[1].type==_VECT){
            gen Eapprox=horner(En,extapprox)/horner(Ed,extapprox);
            gen En2=EE2[0],Ed2=EE2[1];
            vecteur x0=gen2vecteur(Ed*Ed2 % pminv),x1=gen2vecteur(En*Ed2 % pminv),x2=gen2vecteur(En2*Ed % pminv);
            int s=giacmax(x0.size(),giacmax(x1.size(),x2.size()));
            lrdm(x0,s-1); lrdm(x1,s-1); lrdm(x2,s-1);
            if (s>2){
              matrice m=makevecteur(makevecteur(x0[s-1],x1[s-1],x2[s-1]),makevecteur(x0[s-2],x1[s-2],x2[s-2]),makevecteur(x0[s-3],x1[s-3],x2[s-3]));
              if (s>3)
                m.push_back(makevecteur(x0[s-4],x1[s-4],x2[s-4]));
              if (s>4)
                m.push_back(makevecteur(x0[s-5],x1[s-5],x2[s-5]));
              m=mker(m,contextptr);
              if (m.size()==1){
                vecteur k=gen2vecteur(m[0]);
                gen chk=dotvecteur(k,makevecteur(x0,x1,x2));
                if (is_zero(chk)){
                  lcmdeno(k,chk,contextptr);
                  vecteur sols=protect_solve((k[2]*vx_var+k[1])*vx_var+k[0],*vx_var._IDNTptr,1,contextptr);
                  if (sols.size()==2){
                    gen sol0=_evalf(makesequence(sols[0],extpar.digits),contextptr),sol1=_evalf(makesequence(sols[1],extpar.digits),contextptr);
                    if (is_greater(abs(sol0-Eapprox,contextptr),abs(sol1-Eapprox,contextptr),contextptr))
                      e=sols[1];
                    else
                      e=sols[0];
                    return true;
                  }
                }
              } // end m.size()==1
              gen EE3=mreduce_gen(pow(E,3,contextptr),syst,vars,v,varapprox,extpar,contextptr);
              EE3=rur_subst(EE3,vars,nums,diffpmin,pminv,contextptr);
              gen En3,Ed3;
              gen Ed12=Ed*Ed2 % pminv;
              bool chk4=true;
              if (EE3.type==_VECT && EE3[1].type==_VECT){
                // 3rd order check
                En3=EE3[0],Ed3=EE3[1];
                vecteur x0=gen2vecteur(Ed12*Ed3 % pminv),x1=gen2vecteur(En*Ed2*Ed3 % pminv),x2=gen2vecteur(En2*Ed*Ed3 % pminv),x3=gen2vecteur(En3*Ed12 % pminv);
                
                int s=giacmax(giacmax(x0.size(),x1.size()),giacmax(x2.size(),x3.size()));
                lrdm(x0,s-1); lrdm(x1,s-1); lrdm(x2,s-1); lrdm(x3,s-1); 
                if (s>2){
                  matrice m=makevecteur(makevecteur(x0[s-1],x1[s-1],x2[s-1],x3[s-1]),makevecteur(x0[s-2],x1[s-2],x2[s-2],x3[s-2]),makevecteur(x0[s-3],x1[s-3],x2[s-3],x3[s-3]));
                  if (s>3)
                    m.push_back(makevecteur(x0[s-4],x1[s-4],x2[s-4],x3[s-4]));
                  if (s>4)
                    m.push_back(makevecteur(x0[s-5],x1[s-5],x2[s-5],x3[s-5]));
                  m=mker(m,contextptr);
                  if (m.size()==1){
                    vecteur k=gen2vecteur(m[0]);
                    gen chk=dotvecteur(k,makevecteur(x0,x1,x2,x3));
                    if (is_zero(chk)){
                      reverse(k.begin(),k.end());
                      lcmdeno(k,chk,contextptr);
                      negmodpoly(k,k);
                      gen pmin=symb_horner(k,vx_var);
                      *logptr(contextptr) << "Root of " << pmin << "\n";
                      chk4=false; // could be rewritten with Cardan?
                    }
                  }
                }
              }
              // try bisquare
              gen EE4;
              if (chk4){
                EE4=mreduce_gen(pow(E,4,contextptr),syst,vars,v,varapprox,extpar,contextptr);
                EE4=rur_subst(EE4,vars,nums,diffpmin,pminv,contextptr);
              }
              if (chk4 && EE4.type==_VECT && EE4[1].type==_VECT && EE3.type==_VECT && EE3[1].type==_VECT){
                gen En4=EE4[0],Ed4=EE4[1];
                gen Ed34=Ed3*Ed4 % pminv;
                vecteur x0=gen2vecteur(Ed12*Ed34 % pminv),x1=gen2vecteur(En*Ed2*Ed34 % pminv),x2=gen2vecteur(En2*Ed*Ed34 % pminv),x3=gen2vecteur(En3*Ed12*Ed4 % pminv),x4=gen2vecteur(En4*Ed12*Ed3 % pminv);
                int s=giacmax(x4.size(),giacmax(giacmax(x0.size(),x1.size()),giacmax(x2.size(),x3.size())));
                lrdm(x0,s-1); lrdm(x1,s-1); lrdm(x2,s-1); lrdm(x3,s-1); lrdm(x4,s-1);
                if (s>2){
                  matrice m=makevecteur(makevecteur(x0[s-1],x1[s-1],x2[s-1],x3[s-1],x4[s-1]),makevecteur(x0[s-2],x1[s-2],x2[s-2],x3[s-2],x4[s-2]),makevecteur(x0[s-3],x1[s-3],x2[s-3],x3[s-3],x4[s-3]));
                  if (s>3)
                    m.push_back(makevecteur(x0[s-4],x1[s-4],x2[s-4],x3[s-4],x4[s-4]));
                  if (s>4)
                    m.push_back(makevecteur(x0[s-5],x1[s-5],x2[s-5],x3[s-5],x4[s-5]));
                  if (s>5)
                    m.push_back(makevecteur(x0[s-6],x1[s-6],x2[s-6],x3[s-6],x4[s-6]));
                  m=mker(m,contextptr);
                  if (m.size()==2){
                    // FIXME: it's a root of a 3rd order polynomial
                  }
                  if (m.size()==1){
                    vecteur k=gen2vecteur(m[0]);
                    gen chk=dotvecteur(k,makevecteur(x0,x1,x2,x3,x4));
                    if (is_zero(chk)){
                      reverse(k.begin(),k.end());
                      gen shift=k[1]/4;
                      k=taylor(k,shift);
                      if (k[3]==0){
                        vecteur sols=protect_solve((k[0]*vx_var+k[2])*vx_var+k[4],*vx_var._IDNTptr,1,contextptr);
                        if (sols.size()==2){
                          sols[0] = ratnormal(sols[0],contextptr);
                          sols[1] = ratnormal(sols[1],contextptr);
                          gen Eapprox=horner(En,extapprox)/horner(Ed,extapprox);
                          gen Eapproxshift=Eapprox-shift;
                          gen E2approx=Eapproxshift*Eapproxshift;
                          gen sol0=_evalf(makesequence(sols[0] ,extpar.digits),contextptr),sol1=_evalf(makesequence(sols[1],extpar.digits),contextptr);
                          if (is_greater(abs(sol0-E2approx,contextptr),abs(sol1-E2approx,contextptr),contextptr))
                            e=sqrt(sols[1],contextptr);
                          else
                            e=sqrt(sols[0],contextptr);
                          e=normalize_sqrt(e,contextptr);
                          if (!is_positive(Eapproxshift,contextptr))
                            e=-e;
                          e += shift;
                          return true;
                        }
                      }
                    }
                  } // end bisquare m.size()==1
                } // end bisquare s>2
              } // end EE4.type==_VECT
            } // end s>2
          } // end EE2.type==_VECT
        } // end ckdeg2 && Ed.type==_VECT
        // convert back
        En=_poly2symb(makesequence(En,var),contextptr);
        Ed=_poly2symb(makesequence(Ed,var),contextptr);
        if (has_i(Ed)){
          gen End=conj(Ed,contextptr);
          En=_rem(makesequence(En*End,pmin,var),contextptr);
          Ed=_rem(makesequence(Ed*End,pmin,var),contextptr);
        }
      }
    }
    if (failed){
      E=subst(E,vars,inv(diffpmin,contextptr)*nums,false,contextptr);
      // now simplify numerator and denominator
      gen End=_fxnd(E,contextptr);
      if (End.type!=_VECT)
        return false;
      En=End[0]; Ed=End[1];
      En=_rem(makesequence(En,pmin,var),contextptr);
      Ed=_rem(makesequence(Ed,pmin,var),contextptr);
    }
    if (is_zero(En)){
      e=En;
      return true;
    }
    if (0){
      gen EN=_symb2poly(makesequence(En,var),contextptr);
      gen ED=_symb2poly(makesequence(Ed,var),contextptr);
      E=horner_rur(gen2vecteur(EN),sep,syst,vars,contextptr)/horner_rur(gen2vecteur(ED),sep,syst,vars,contextptr);
      e=subst(E,vars,v,false,contextptr);
      return true;
    }
    // multiply denominator by conjugate
    gen bez=_egcd(makesequence(Ed,pmin,var),contextptr);
    if (bez.type!=_VECT || bez._VECTptr->size()!=3)
      return false;
    // bez[0]*Ed+bez[1]*pmin=bez[2]
    // hence En/Ed=En*bez[0]/bez[2]
    En=_rem(makesequence(En*bez[0],pmin,var),contextptr);
    gen EN=_symb2poly(makesequence(En,var),contextptr);
    if (!rootofallowed){
      E=horner_rur(gen2vecteur(EN),sep,syst,vars,contextptr,bez[2]);
      e=subst(E,vars,v,false,contextptr);
      return true;
    }
    E=horner(EN,ext)/bez[2];
    if (!ckdeg2){
      if (0 && E.type==_EXT) // should remains _EXT if called to generate ggn before ext factorization
        e=symb_rootof(*E._EXTptr,*(E._EXTptr+1),contextptr);
      else
        e=E;
      return true;
    }
    //E=algebraic_EXTension(EN,pminv)/bez[2];
    E=r2e(E,vecteur(1,vecteur(0)),contextptr);
    bool has_rootof=!lop(E,at_rootof).empty();
    bool b=calc_mode(contextptr)==1 || abs_calc_mode(contextptr)==38;
    if (b || (has_rootof && lop(v,at_rootof).empty()) ){
      E=horner_rur(gen2vecteur(EN),sep,syst,vars,contextptr,bez[2]);
      e=subst(E,vars,v,false,contextptr);
      return true;
    }
    e=E;
    return true;
  }

  // returns 0 on failure, or n==v.size() on success
  // find Q algebraic extensions inside e, on success
  // rewrite e as E in terms of
  // symbolic identifiers contained in list vars (effective values in v)
  // where vars satisfy syst
  // G is a rur for syst
  int algnum_convert(const gen & e_,gen & E,gen & G,vecteur & syst,vecteur & vars,vecteur & v,vecteur & varapprox,ext_param_t & extpar,GIAC_CONTEXT){
    double oldeps=epsilon(contextptr);
    // return 0: // until it's fixed
    gen e(e_);
    gen ef;
    if (!has_evalf(e,ef,1,contextptr)) 
      return 0;
    v.clear();
    alg_lvar(e,v);
    if (v.size()!=1 || v.front().type!=_VECT || !v.front()._VECTptr->empty())
      return 0;
    v=lvar(e);
    vecteur vr,wr;
    // rewrite rootofs inside e
    for (int i=0;i<v.size();++i){
      if (v[i].is_symb_of_sommet(at_rootof)){
        gen f=v[i]._SYMBptr->feuille;
        if (f.type==_VECT && f._VECTptr->size()==2){
          vr.push_back(v[i]);
          gen p=f._VECTptr->front(),pmin=f._VECTptr->back();
          if (p.type==_VECT && pmin.type==_VECT && p._VECTptr->size()>=pmin._VECTptr->size())
            p=p%pmin;
          pmin.subtype=_POLY1__VECT;
          wr.push_back(horner(p,symbolic(at_rootof,pmin)));
        }
      }
    }
    if (!vr.empty()){
      e=subst(e,vr,wr,true,contextptr);
      v=lvar(e);
    }
    recursive_lvar(v);
    bool rootofallowed=false;//true;
    int n=v.size();
    if (n<3
        && 0 // n!=1
        )
      return 0;
    v=sort2(v);
    if (extpar.chkext)
      varapprox=gen2vecteur(algnum_evalf(v,extpar.digits,contextptr));
    else
      varapprox=v;
    epsilon(oldeps,contextptr);
    vars.resize(n);
    for (int i=0;i<n;++i){
      gen g("x"+print_INT_(i+1),contextptr);
      vars[i]=g;
    }
    vecteur V(v),VARS(vars);
    E=subst(e,v,vars,false,contextptr);
    syst.clear(); 
    G=0;
    gen x0("x0",contextptr);
    bool cplx=has_i(E),cplxv=has_i(v);
    if (cplx){
      if (cplxv){
        //v=subst(v,cst_i,x0,false,contextptr); V=v;
        v.insert(v.begin(),cst_i);
        vars.insert(vars.begin(),x0);
        varapprox.insert(varapprox.begin(),cst_i);
      }
      else {
        v.push_back(cst_i);
        vars.push_back(x0);
        varapprox.push_back(cst_i);
      }
      ++n;
    }
    int dbg=debug_infolevel;
    int algext_order=1,algext_cur=0;
    if (dbg)
      debug_infolevel--;
    for (int i=0;i<n;++i){
      gen gg=undef;
      if (cplx && v[i]==cst_i){
        syst.push_back(pow(vars[i],2,contextptr)+1);
        gg=vx_var*vx_var+1;
        if (i==0){
          E=subst(E,cst_i,vars[0],false,contextptr);
        }
        algext_cur=2;
        algext_order*=2;
      }
      else if (v[i].is_symb_of_sommet(at_rootof)){
        gen g=v[i]._SYMBptr->feuille,p,pmin;
        if (g.type!=_VECT || g._VECTptr->size()!=2){
          p=makevecteur(1,0);
          pmin=g;
          algext_cur = gen2vecteur(pmin).size()-1;
          algext_order *= algext_cur;
        }
        else {
          p=g[0];
          pmin=g[1];
          algext_cur = gen2vecteur(pmin).size()-1;;
          algext_order *= algext_cur;
        }
        p=horner(p,vars[i]);
        E=subst(E,vars[i],p,false,contextptr);
        gg=horner(pmin,vx_var);
        pmin=horner(pmin,vars[i]);
        syst.push_back(pmin);
      }
      else if (v[i].is_symb_of_sommet(at_pow)){
        gen g=v[i]._SYMBptr->feuille;
        if (g.type!=_VECT || g._VECTptr->size()!=2){
          debug_infolevel=dbg;
          return 0;
        }
        gen base=g[0], expo=g[1],num,d;
        fxnd(expo,num,d);
        algext_cur=d.val;
        algext_order *= d.val;
        gg=symb_pow(vx_var,d)-pow(g[0],num,contextptr);
        base=subst(base,V,VARS,true,contextptr); // DOIT base should be reduced wrt syst
        if (cplxv)
          base=subst(base,cst_i,x0,true,contextptr);
        syst.push_back(symb_pow(vars[i],d)-pow(base,num,contextptr));
        // if gg is not irreducible, select the right factor
        gen vi=_evalf(makesequence(v[i],extpar.digits),contextptr);
        if (vi.type<_IDNT){
          gen ggf=_factors(gg,contextptr);
          if (ggf.type==_VECT && ggf._VECTptr->size()>2){
            vecteur & ggl=*ggf._VECTptr;
            gen oldP=ggl[0],oldval=abs(_horner(makesequence(oldP,vi,vx_var),contextptr),contextptr);
            for (int i=2;i<ggl.size();i+=2){
              gen curP=ggl[i],curval=abs(_horner(makesequence(curP,vi,vx_var),contextptr),contextptr);
              if (is_greater(oldval,curval,contextptr)){
                oldval=curval;
                oldP=curP;
              }
            }
            gg=oldP;
            vecteur PV=gen2vecteur(_coeff(makesequence(oldP,vx_var),contextptr));
            if (PV.size()>=2){
              int I=1;
              for (;I<PV.size()-1;++I){
                if (!is_exactly_zero(PV[I]))
                  break;
              }
              if (I==PV.size()-1)
                v[i]=pow(-PV[I]/PV[0],inv(I,contextptr),contextptr);
            }
            oldP=subst(oldP,vx_var,vars[i],false,contextptr);
            syst.back()=subst(oldP,V,VARS,false,contextptr);
            if (cplxv)
              syst.back()=subst(syst.back(),cst_i,x0,true,contextptr);              
          }
        }
      }
      else {
        debug_infolevel=dbg;
        return 0;
      }
      if (!extpar.chkext && algext_order>MAX_COMMON_ALG_EXT_ORDER_SIZE){
        extpar.chkext=algext_order;
        varapprox=gen2vecteur(algnum_evalf(v,extpar.digits,contextptr));
        epsilon(oldeps,contextptr);
      }
      if ( (!extpar.chkext || algext_order<extpar.chkext) && i){
#if 1 // if changed to 0, #if below must be changed too
        // check if the last extension is required
        vecteur prevvars(vars.begin(),vars.begin()+i);
        vecteur prevsyst(syst); prevsyst.pop_back();
        gen sep,pmin;
        if (G.type==_VECT){ // i>=2
          sep=G[1]; pmin=G[2];
        }
        else {
          sep=vars[0]; pmin=syst[0];
        }
        // code with ext factorization
        // last equation of syst will split using previous variables
        gen var=lidnt(pmin)[0];
        gen curpmin=_symb2poly(makesequence(pmin,var),contextptr);
        if (curpmin.type!=_VECT){
          debug_infolevel=dbg;
          return 0;
        }
        if (curpmin._VECTptr->front()==-1){
          curpmin=-curpmin;
        }
        gen ext=algebraic_EXTension(makevecteur(1,0),*curpmin._VECTptr);
        gen ggn,ggd;
        gen extapprox=subst(sep,vars,v,false,contextptr);
        extapprox=_evalf(makesequence(extapprox,extpar.digits),contextptr);
        // beware that evalf(ext) might return a wrong value
        // gen extapprox=_evalf(makesequence(ext,extpar.digits),contextptr);
        epsilon(oldeps,contextptr);
        gen p=_symb2poly(makesequence(syst.back(),vars[i]),contextptr);
        if (G.type==_VECT){
          vecteur nums=vecteur(G._VECTptr->begin()+4,G._VECTptr->end());
          vecteur pminv=*curpmin._VECTptr;
          if (algnum_rewritable(p,prevsyst,prevvars,vecteur(v.begin(),v.begin()+i),varapprox,nums,G[3],pminv,pmin,var,ext,extapprox,sep,true/* rootof allowed */,false/* ckdeg2*/,extpar,ggn,contextptr) && ggn.type==_VECT){
            ggn=poly12polynome(*ggn._VECTptr);
          }
        }
        if (ggn.type!=_POLY) {
          gen ro=rootof(curpmin,contextptr);
          gg=e2r(makevecteur(ro,gg),vecteur(1,vecteur(1,vx_var)),contextptr);
          gg=gg[1];
          fxnd(gg,ggn,ggd);
        }
        if (ggn.type!=_POLY){
          debug_infolevel=dbg;              
          return 0;
        }
        gen an(1),extra_div(1); polynome p_content; factorization f;
        if (dbg)
          CERR << CLOCK()*1e-6 << " ext_factor start\n";
        //debug_infolevel=dbg;              
        if (!ext_factor(*ggn._POLYptr,ext,an,p_content,f,false,extra_div)){
          debug_infolevel=dbg;              
          return 0;
        }
        //--debug_infolevel;
        if (dbg)
          CERR << CLOCK()*1e-6 << " ext_factor end\n";
        if (f.size()>1){
          // select the right factor in f
          gen vival=_evalf(makesequence(v[i],extpar.digits),contextptr);
          gen curerr(undef); vecteur curP;
          for (int j=0;j<f.size();++j){
            polynome & pcur=f[j].fact;
            modpoly P=polynome2poly1(pcur,1),Papprox(P);
            // rewrite extensions in P using original sep
            for (int i=0;i<P.size();++i){
              gen & Pi=Papprox[i];
              if (Pi.type==_FRAC && Pi._FRACptr->num.type==_EXT)
                Papprox[i]=horner(*Pi._FRACptr->num._EXTptr,extapprox)/Pi._FRACptr->den;
              else if (Pi.type==_EXT)
                Papprox[i]=horner(*Pi._EXTptr,extapprox);
            }
            gen err=abs(horner(Papprox,vival),contextptr);
            if (is_undef(curerr) && is_greater(1e-10,err,contextptr)){
              curP=P;
              curerr=err;
            }
            else {
              if (is_greater(curerr,err,contextptr)){
                curP=P;
                curerr=err;
              }
            }
          }
          if (is_undef(curerr)){
            debug_infolevel=dbg;              
            return 0;
          }
          if (curP.size()==2){
            gen curval=-curP[1]/curP[0];
            fxnd(curval,ggn,ggd);
            if (ggn.type==_EXT && ggn._EXTptr->type==_VECT){
              // vars[i] will be replaced using sep
              //gen dep=horner(*ggn._EXTptr->_VECTptr,sep)/ggd;
              gen dep=horner_rur(*ggn._EXTptr->_VECTptr,sep,vecteur(syst.begin(),syst.end()-1),vecteur(vars.begin(),vars.begin()+i),contextptr,ggd);
              gen dep2=subst(dep,vars,v,false,contextptr);
              E=subst(E,vars[i],dep,false,contextptr);
              VARS=subst(VARS,vars[i],dep,false,contextptr);
              V=subst(V,v[i],dep2,true,contextptr);
              v=subst(v,v[i],dep2,true,contextptr);
              vars.erase(vars.begin()+i); v.erase(v.begin()+i);
              syst.pop_back();
              --i; --n;
              algext_order/=algext_cur;
              continue;
            }
          } // end variable can be eliminated
          // replace syst.back() using curP
          // code not tested, needs an example
          for (int i=0;i<curP.size();++i){
            fxnd(curP[i],ggn,ggd);
            if (ggn.type==_EXT && ggn._EXTptr->type==_VECT){
              // vars[i] will be replaced using sep
              //gen dep=horner(*ggn._EXTptr->_VECTptr,sep)/ggd;
              gen dep=horner_rur(*ggn._EXTptr->_VECTptr,sep,vecteur(syst.begin(),syst.end()-1),vecteur(vars.begin(),vars.begin()+i),contextptr,ggd);
              curP[i]=dep;
            }
          }
          syst.back()=r2e(curP,vars[i],contextptr);
        } // end not irreducible (f.size()>1)
        else {
          if (cplx && v[i]==cst_i){
            vars.pop_back(); v.pop_back(); syst.pop_back(); cplx=false;
            --i; --n; 
            continue;
          }
        }
        if (dbg)
          CERR << CLOCK()*1e-6 << " gbasis start\n";
        vecteur curvars(vars.begin(),vars.begin()+i+1);
        G=_gbasis(makesequence(syst,curvars,change_subtype(_RUR_REVLEX,_INT_GROEBNER)),contextptr);
        if (G.type!=_VECT || G._VECTptr->size()!=curvars.size()+4 || G._VECTptr->front().type!=_INT_ || G._VECTptr->front().val!=_RUR_REVLEX){
          debug_infolevel=dbg;
          return 0;
        }
        if (dbg)
          CERR << CLOCK()*1e-6 << " gbasis end, sep=" << sep << " vars=" << prevvars << "\n";
        // end code with ext factorization
#else
          // check if the last extension is required
        vecteur curvars(vars.begin(),vars.begin()+i+1);
        prevG=G;
        if (dbg)
          CERR << CLOCK()*1e-6 << " gbasis start\n";
        G=_gbasis(makesequence(syst,curvars,change_subtype(_RUR_REVLEX,_INT_GROEBNER)),contextptr);
        if (G.type!=_VECT || G._VECTptr->size()!=curvars.size()+4 || G._VECTptr->front().type!=_INT_ || G._VECTptr->front().val!=_RUR_REVLEX){
          debug_infolevel=dbg;
          return 0;
        }
        gen sep=G[1],pmin=G[2];
        if (dbg)
          CERR << CLOCK()*1e-6 << " gbasis end, sep=" << sep << " vars=" << curvars << "\n";
        if (_is_irreducible(pmin,contextptr)==0){
          if (dbg)
            CERR << CLOCK()*1e-6 << " reducible\n";
          bool done=false;
          if (0){
            // extract curvars.back() using pmin(sep)==0
            // note that it may returns large expressions compared to ext factor
            // like (-3012*x0*x1^5+2775*x1^4+1875*x0*x1^3+300*x1^2+3753*x0*x1-6045)/(x0*(x1^2*(-1251*x1^2+3012)-1875)+x1*(x1^2*(-100*x1^2+2015)-2775)) instead of x1^3 (x0=i, x1=3^(1/6))
            vecteur pminv;
            gen var=lvar(pmin)[0];
            vecteur pminfact=factors(pmin,var,contextptr);
            int pmins=pminfact.size();
            pmin=pminfact[0];
            gen curpmin=_symb2poly(makesequence(pmin,var),contextptr);
            if (curpmin.type!=_VECT)
              return 0;
            pminv=*curpmin._VECTptr;
            gen rexact=subst(sep,vars,v,false,contextptr);
            gen r=_evalf(makesequence(rexact,extpar.digits),contextptr);
            if (pmins>2){
              gen curmin=abs(horner(pminv,r),contextptr);
              for (int j=2;j<pmins;j+=2){
                gen curpmin=_symb2poly(makesequence(pminfact[j],var),contextptr);
                if (curpmin.type!=_VECT)
                  continue;
                gen curval=abs(horner(*curpmin._VECTptr,r),contextptr);
                if (is_greater(curmin,curval,contextptr)){
                  pmin=curpmin;
                  curmin=curval;
                  pminv=*curpmin._VECTptr;
                }
              }
            }
            if (!is_positive(pminv[0],contextptr))
              pminv=-pminv;
            gen dep=horner_rur(pminv,sep,syst,curvars,contextptr,1);
            // find gcd with syst.back()
            gen deg=_degree(makesequence(dep,curvars.back()),contextptr);
            if (deg==1){
              dep=solve(dep,curvars.back(),0,contextptr)[0];
              done=true;
            }
            if (done){
              gen dep2=subst(dep,vars,v,false,contextptr);
              E=subst(E,vars[i],dep,false,contextptr);
              VARS=subst(VARS,vars[i],dep,false,contextptr);
              V=subst(V,v[i],dep2,false,contextptr);
              v=subst(v,v[i],dep2,false,contextptr);
              vars.erase(vars.begin()+i); v.erase(v.begin()+i);
              syst.pop_back();
              --i; --n;
              G=prevG;
            }
          }
          if (!done) {
            // code with ext factorization
            // last equation of syst will split using previous variables
            if (prevG==0){
              pmin=syst[0]; sep=lidnt(pmin)[0];
            }
            else {
              sep=prevG[1];pmin=prevG[2];
            }
            gen var=lidnt(pmin)[0];
            gen curpmin=_symb2poly(makesequence(pmin,var),contextptr);
            if (curpmin.type!=_VECT){
              debug_infolevel=dbg;
              return 0;
            }
            if (curpmin._VECTptr->front()==-1){
              curpmin=-curpmin;
            }
            gen ro=rootof(curpmin,contextptr);
            gen ext=algebraic_EXTension(makevecteur(1,0),*curpmin._VECTptr);
            gen ggn,ggd;
            if (prevG!=0){
              gen extapprox=_evalf(makesequence(ext,extpar.digits),contextptr);
              gen p=_symb2poly(makesequence(syst.back(),vars[i]),contextptr);
              vecteur prevvars(curvars),prevsyst(syst); prevvars.pop_back(); prevsyst.pop_back();
              vecteur nums=vecteur(prevG._VECTptr->begin()+4,prevG._VECTptr->end());
              vecteur pminv=*curpmin._VECTptr;
              if (algnum_rewritable(p,prevsyst,prevvars,vecteur(v.begin(),v.begin()+i),vecteur(varapprox.begin(),varapprox.begin()+i),nums,prevG[3],pminv,pmin,var,ext,extapprox,sep,true/* rootof allowed */,false/* ckdeg2*/,ggn,contextptr) && ggn.type==_VECT){
                ggn=poly12polynome(*ggn._VECTptr);
              }
            }
            if (ggn.type!=_POLY) {
              gg=e2r(makevecteur(ro,gg),vecteur(1,vecteur(1,vx_var)),contextptr);
              gg=gg[1];
              fxnd(gg,ggn,ggd);
            }
            if (ggn.type==_POLY){
              gen an(1),extra_div(1); polynome p_content; factorization f;
              if (dbg)
                CERR << CLOCK()*1e-6 << " ext_factor start\n";
              if (!ext_factor(*ggn._POLYptr,ext,an,p_content,f,false,extra_div)){
                debug_infolevel=dbg;              
                return 0;
              }
              if (dbg)
                CERR << CLOCK()*1e-6 << " ext_factor end\n";
              // if (f.size()!=d) return 0;
              // select the right factor in f
              gen vival=_evalf(makesequence(v[i],extpar.digits),contextptr);
              gen curval(undef),curerr,err;
              for (int j=0;j<f.size();++j){
                polynome & pcur=f[j].fact;
                modpoly P=polynome2poly1(pcur,1);
                if (P.size()!=2)
                  continue;
                gen cur=-P[1]/P[0];
                gen curf=_evalf(makesequence(cur,extpar.digits),contextptr);
                gen err=abs(curf-vival,contextptr);
                if (is_undef(curval) && is_greater(1e-10,err,contextptr)){
                  curval=cur;
                  curerr=err;
                }
                else {
                  if (is_greater(curerr,err,contextptr)){
                    curval=cur;
                    curerr=err;
                  }
                }
              }
              if (is_undef(curval)){
                debug_infolevel=dbg;              
                return 0;
              }
              fxnd(curval,ggn,ggd);
              if (ggn.type==_EXT && ggn._EXTptr->type==_VECT){
                // vars[i] will be replaced using sep
                //gen dep=horner(*ggn._EXTptr->_VECTptr,sep)/ggd;
                gen dep=horner_rur(*ggn._EXTptr->_VECTptr,sep,vecteur(syst.begin(),syst.end()-1),vecteur(vars.begin(),vars.begin()+i),contextptr,ggd);
                gen dep2=subst(dep,vars,v,false,contextptr);
                E=subst(E,vars[i],dep,false,contextptr);
                VARS=subst(VARS,vars[i],dep,false,contextptr);
                V=subst(V,v[i],dep2,true,contextptr);
                v=subst(v,v[i],dep2,true,contextptr);
                vars.erase(vars.begin()+i); v.erase(v.begin()+i);
                syst.pop_back();
                --i; --n;
                G=prevG;
              }
            }
          } // end code with ext factorization
        } else {
          if (cplx && v[i]==cst_i){
            vars.pop_back(); v.pop_back(); syst.pop_back(); cplx=false;
            --i; --n; G=prevG;
            continue;
          }
          //rootofallowed=false;
          if (!rootofallowed) continue;
          gen var=lidnt(pmin)[0];
          gen curpmin=_symb2poly(makesequence(pmin,var),contextptr);
          if (curpmin.type!=_VECT){
            debug_infolevel=dbg;            
            return 0;
          }
          if (curpmin._VECTptr->front()==-1){
            curpmin=-curpmin;
          }
          // pmin is irreducible
          // but sep is not necessarily the largest root of pmin
          // we must adjust if possible, factor pmin over pmin extension
          // and replace in E vars[i] by the root in terms of the extension
          gen an(1),extra_div(1); polynome p_content; factorization f;
          gen ext=algebraic_EXTension(makevecteur(1,0),*curpmin._VECTptr);
          gen vival=_evalf(makesequence(subst(sep,vars,v,false,contextptr),extpar.digits),contextptr);
          if (is_greater(abs(vival-ext,contextptr),1e-10,contextptr)){
            if (curpmin._VECTptr->size()>17){
              rootofallowed=false; continue;
            }
            polynome p(poly12polynome(*curpmin._VECTptr));
            if (!ext_factor(p,ext,an,p_content,f,false,extra_div)){
              debug_infolevel=dbg;              
              return 0;
            }
            // if (f.size()!=d) return 0;
            // select the right factor in f
            gen curval(undef),curerr,err;
            for (int j=0;j<f.size();++j){
              polynome & pcur=f[j].fact;
              modpoly P=polynome2poly1(pcur,1);
              if (P.size()!=2)
                continue;
              gen cur=-P[1]/P[0],err=abs(cur-vival,contextptr);
              if (is_undef(curval) && is_greater(1e-10,err,contextptr)){
                curval=cur;
                curerr=err;
              }
              else {
                if (is_greater(curerr,err,contextptr)){
                  curval=cur;
                  curerr=err;
                }
              }
            }
            if (is_undef(curval)){
              rootofallowed=false;
              continue;
            }
            gen ggn,ggd; fxnd(curval,ggn,ggd);
            if (ggn.type==_EXT && ggn._EXTptr->type==_VECT){
              // vars[i] will be replaced using sep
              gen dep=horner(*ggn._EXTptr->_VECTptr,sep)/ggd;
              gen dep2=algebraic_EXTension(makevecteur(1,0),curpmin);
              dep2=r2e(dep2,vecteur(1,vecteur(0)),contextptr);
              E=subst(E,vars[i],dep,false,contextptr);
              V=subst(V,v[i],dep2,true,contextptr);
              v=subst(v,v[i],dep2,true,contextptr);
              syst[i]=pmin;
              --i;
              continue;
            }
          } // end v[i] was not the largest root of pmin
        } // end pmin irreducible
#endif
      } // end i>0
    } // end for loop on i
    if (n==1){
      gen var=vars[0],eq=syst[0];
      G=makevecteur(_RUR_REVLEX,var,eq,1,var);
    }
    if (dbg) CERR << CLOCK()*1e-6 << " End constructing common algebraic extension\n";
    debug_infolevel=dbg;
    return n;
  }

  // redtype=0 (rref fullreduction), 1 (rref upper), 2 (det), 3 (lu)
  bool algnum_rref(const matrice & a, matrice & res, vecteur & pivots, gen & det,int redtype,GIAC_CONTEXT){
    gen E,G;
    vecteur syst,vars,v,varapprox;
    ext_param_t extpar={LAZY_ALG_EXT,ALG_EXT_DIGITS,4096};
    int n=algnum_convert(a,E,G,syst,vars,v,varapprox,extpar,contextptr);
    if (n<2)
      return false;
    vector<polynome> systnums; vecteur lv; vector< vector<int> > transpositions; polynome tmpn; gen tmpD;
    if (!prepare(E[0][0],syst,vars,systnums,v,varapprox,transpositions,extpar,lv,tmpn,tmpD,contextptr))
      return false;
    gen EE=e2r(E,lv,contextptr),EN,ED;
    fxnd(EE,EN,ED);
    if (!ckmatrix(EE))
      return false;
    res=*EE._VECTptr;
    int L=res.size(),C=res.front()._VECTptr->size();
    vecteur permu(L);
    for (int i=0;i<L;++i)
      permu[i]=i;
    det=inv(pow(ED,L,contextptr),contextptr);
    int l=0,c=0;
    for (;l<L && c<C;){
      // find pivot
      gen p=res[l][c];
      if (algnum_rref_is_zero(p,syst,lv,systnums,v,varapprox,transpositions,extpar,contextptr)){
        for (int i=l+1;i<=L;++i){
          if (i==L){
            if (redtype==2){
              det=0;
              return true;
            }
            ++c;
            if (c==C)
              break;
            i=l;
          }
          p=res[i][c];
          if (!algnum_rref_is_zero(p,syst,lv,systnums,v,varapprox,transpositions,extpar,contextptr)){
            res[i]._VECTptr->swap(*res[l]._VECTptr);
            det=-det;
            swapgen(permu[i],permu[l]);
            break;
          }
        }
        if (c==C)
          break;
      }
      pivots.push_back(p);
      det=algnum_reduce_rref(p*det,syst,lv,systnums,v,varapprox,transpositions,extpar,contextptr);
      fxnd(p,EN,ED);
      p=ED/EN;
      // reduce line (0 or l+1) to L
      vecteur & pivotline=*res[l]._VECTptr;
      for (int i=redtype?l+1:0;i<L;++i){
        if (i==l)
          continue;
        vecteur & curline=*res[i]._VECTptr;
        gen cur=p*curline[c];
        cur=algnum_reduce_rref(cur,syst,lv,systnums,v,varapprox,transpositions,extpar,contextptr);
        if (redtype<2)
          curline[c]=0;
        else if (redtype==3)
          curline[c]=cur;
        for (int j=c+1;j<C;++j){
          curline[j]=algnum_reduce_rref(curline[j]-cur*pivotline[j],syst,lv,systnums,v,varapprox,transpositions,extpar,contextptr);
        }
      }
      if (redtype==0){ // divide pivotline by pivot
        for (int j=c+1;j<C;++j)
          pivotline[j]=algnum_reduce_rref(p*pivotline[j],syst,lv,systnums,v,varapprox,transpositions,extpar,contextptr);
        pivotline[c]=1;
      }
      ++l; ++c;
    }
    gen sep,var,pmin,diffpmin,curpmin,rexact,ext,r; vecteur pminv,nums;
    bool rootofallowed=false;
    if (!extpar.chkext && G.type==_VECT){
      sep=G[1]; pmin=G[2]; diffpmin=G[3];
      var=lvar(pmin)[0];
      curpmin=_symb2poly(makesequence(pmin,var),contextptr);
      if (curpmin.type!=_VECT)
        return false;
      pminv=*curpmin._VECTptr;
      rexact=subst(sep,vars,v,false,contextptr);
      if (!rootofallowed){
        rootofallowed=!lop(v,at_rootof).empty();
      }
      ext=rootofallowed?algebraic_EXTension(makevecteur(1,0),pminv):rexact;
      r=_evalf(makesequence(rexact,extpar.digits),contextptr);
      nums=vecteur(G._VECTptr->begin()+4,G._VECTptr->end());
      // rewrite E as a rational frac in var, replace vars by nums/diffpmin
      pmin=horner(pminv,var);
    }
    E=mreduce_back(det,lv,contextptr);
    if (!algnum_rewritable(E,syst,vars,v,varapprox,nums,diffpmin,pminv,pmin,var,ext,r,sep,rootofallowed,!rootofallowed/* ckdeg2*/,extpar,det,contextptr))
      return false;
    if (redtype==2)
      return true;
    if (redtype==3)
      pivots.push_back(permu);
    for (int i=0;i<L;++i){
      vecteur & resi=*res[i]._VECTptr;
      for (int j=0;j<C;++j)
        resi[j]=mreduce_back(resi[j],lv,contextptr);
    }
    if (!algnum_rewritable(res,syst,vars,v,varapprox,nums,diffpmin,pminv,pmin,var,ext,r,sep,rootofallowed,!rootofallowed/* ckdeg2*/,extpar,E,contextptr) || !ckmatrix(E))
      return false;
    res=*E._VECTptr;
    return true;
  }

  bool algnum_cleanup(gen & e,GIAC_CONTEXT){
    if (e.type==_EXT) {
      e=symb_rootof(*e._EXTptr,*(e._EXTptr+1),contextptr);
      return true;
    }
    if (e.type==_FRAC){
      if (algnum_cleanup(e._FRACptr->num,contextptr)){
        e=e._FRACptr->num/e._FRACptr->den;
        return true;
      }
    }
    return false;
  }

  // detect if e is in an algebraic extension of Q, simplifies
  bool algnum_normal(gen & e,GIAC_CONTEXT){
#ifdef USE_GMP_REPLACEMENTS
    return false;
#endif
    e=normalize_sqrt(e,contextptr); 
    gen E,G;
    vecteur syst,vars,v,varapprox;
    ext_param_t extpar={LAZY_ALG_EXT,ALG_EXT_DIGITS,4096};
    if (!algnum_convert(e,E,G,syst,vars,v,varapprox,extpar,contextptr))
      return false;
    int n=syst.size();
    if (extpar.chkext==0 && n>9){
      *logptr(contextptr) << "Algebraic extension degree too large!\n";
      e=simplifier(ratnormal(e,contextptr),contextptr);
      return true;
    }
    bool rootofallowed=false;
#if 1
    gen sep,var,pmin,diffpmin,curpmin,rexact,ext,r; vecteur pminv,nums;
    if (!extpar.chkext && G.type==_VECT){
      sep=G[1]; pmin=G[2]; diffpmin=G[3];
      var=lvar(pmin)[0];
      curpmin=_symb2poly(makesequence(pmin,var),contextptr);
      if (curpmin.type!=_VECT)
        return false;
      pminv=*curpmin._VECTptr;
      rexact=subst(sep,vars,v,false,contextptr);
      if (!rootofallowed){
        rootofallowed=!lop(v,at_rootof).empty();
      }
      ext=rootofallowed?algebraic_EXTension(makevecteur(1,0),pminv):rexact;
      r=_evalf(makesequence(rexact,extpar.digits),contextptr);
      nums=vecteur(G._VECTptr->begin()+4,G._VECTptr->end());
      // rewrite E as a rational frac in var, replace vars by nums/diffpmin
      pmin=horner(pminv,var);
    }
    if (!algnum_rewritable(E,syst,vars,v,varapprox,nums,diffpmin,pminv,pmin,var,ext,r,sep,rootofallowed,!rootofallowed/* ckdeg2*/,extpar,e,contextptr))
      return false;
    algnum_cleanup(e,contextptr);
    return true;
#else
    G=_gbasis(makesequence(syst,vars,change_subtype(_RUR_REVLEX,_INT_GROEBNER)),contextptr);
    if (dbg) CERR << CLOCK()*1e-6 << " End constructing common algebraic extension\n";
    debug_infolevel=dbg;
    if (G.type==_VECT && G._VECTptr->size()==vars.size()+4 && G._VECTptr->front().type==_INT_ && G._VECTptr->front().val==_RUR_REVLEX){
      gen sep=G[1],var,pmin=G[2],diffpmin=G[3]; vecteur pminv;
      var=lvar(pmin)[0];
      vecteur pminfact=factors(pmin,var,contextptr);
      int pmins=pminfact.size();
      pmin=pminfact[0];
      gen curpmin=_symb2poly(makesequence(pmin,var),contextptr);
      if (curpmin.type!=_VECT)
        return false;
      pminv=*curpmin._VECTptr;
      gen rexact=subst(sep,vars,v,false,contextptr);
      gen r=_evalf(makesequence(rexact,extpar.digits),contextptr);
      if (pmins>2){
        gen curmin=abs(horner(pminv,r),contextptr);
        for (int j=2;j<pmins;j+=2){
          gen curpmin=_symb2poly(makesequence(pminfact[j],var),contextptr);
          if (curpmin.type!=_VECT)
            continue;
          gen curval=abs(horner(*curpmin._VECTptr,r),contextptr);
          if (is_greater(curmin,curval,contextptr)){
            pmin=curpmin;
            curmin=curval;
            pminv=*curpmin._VECTptr;
          }
        }
      }
      if (!is_positive(pminv[0],contextptr))
        pminv=-pminv;
      if (pminv.size()>MAX_COMMON_ALG_EXT_ORDER_SIZE+1){
        *logptr(contextptr) << "Algebraic extension degree too large " << pminv.size()-1 << " max is " << MAX_COMMON_ALG_EXT_ORDER_SIZE << "\n";
        e=simplifier(ratnormal(e,contextptr),contextptr);
        return true;
      }
      gen ext=rootofallowed?algebraic_EXTension(makevecteur(1,0),pminv):rexact;
      // now check that r is the max root of pminv, otherwise
      // it's not rootof([1,0],pminv)
      gen R=rootofallowed?accurate_evalf(ext,100):r;
      if (rootofallowed && is_greater(abs(1-r/R,contextptr),1e-10,contextptr)){
        if (pminv.size()>17)
          rootofallowed=false;
        else {
          // factor pminv over ext, and see if r is a root of one factor
          // this will automatically handle sqrt(2-sqrt(2)) 
          polynome p(poly12polynome(pminv));
          gen an(1),extra_div(1); polynome p_content; factorization f;
          if (!ext_factor(p,ext,an,p_content,f,false,extra_div))
            return false;
          gen curval(undef),curerr,err;
          for (int j=0;j<f.size();++j){
            polynome & pcur=f[j].fact;
            modpoly P=polynome2poly1(pcur,1);
            if (P.size()!=2)
              continue;
            gen cur=-P[1]/P[0],err=abs(cur-r,contextptr);
            if (is_undef(curval) && is_greater(1e-10,err,contextptr)){
              curval=cur;
              curerr=err;
            }
            else {
              if (is_greater(curerr,err,contextptr)){
                curval=cur;
                curerr=err;
              }
            }
          }
          if (is_undef(curval))
            return false;
          // if we are here, r is curval
          ext=curval;
        }
      }
      vecteur nums=vecteur(G._VECTptr->begin()+4,G._VECTptr->end());
      // rewrite E as a rational frac in var, replace vars by nums/diffpmin
      pmin=horner(pminv,var);
      if (!algnum_rewritable(E,syst,vars,v,varapprox,nums,diffpmin,pminv,pmin,var,ext,r,sep,rootofallowed,true/* ckdeg2*/,e,contextptr))
        return false;
      algnum_cleaup(e,contextptr);
      return true;
    }
#endif
    return false;
  }

#endif // algnum available

  gen recursive_normal(const gen & e,bool distribute_div,GIAC_CONTEXT){
#ifdef TIMEOUT
    control_c();
#endif
    if (e.type<=_CPLX) return e;
    if (is_inequation(e) && e._SYMBptr->feuille.type==_VECT){
      vecteur & v=*e._SYMBptr->feuille._VECTptr;
      unary_function_ptr u=e._SYMBptr->sommet;
      if (calc_mode(contextptr)==1){
	if (lidnt(v[0]).empty() && !lidnt(v[1]).empty()){
	  gen e2=makesequence(v[1],v[0]);
	  if (u==at_superieur_strict)
	    return symbolic(at_inferieur_strict,e2);
	  if (u==at_superieur_egal)
	    return symbolic(at_inferieur_egal,e2);
	  if (u==at_inferieur_strict)
	    return symbolic(at_superieur_strict,e2);
	  if (u==at_inferieur_egal)
	    return symbolic(at_superieur_egal,e2);
	}
	return e;
      }
      if (v[0].type==_IDNT && equalposcomp(lidnt(v[1]),v[0])==0)
	return symbolic(u,makesequence(v[0],normal(v[1],true,contextptr)));
      gen e2= v[0]-v[1];
      e2=normal(e2,true,contextptr);
      gen c=_content(e2,contextptr);
      if (is_integer(c) || c.type==_FRAC){
	e2=ratnormal(e2/c,contextptr);
	if (is_positive(-c,contextptr))
	  e2=-e2;
      }
      gen l=_lcoeff(e2,contextptr);
      if ( is_integer(l) || l.type==_FRAC){
	e2=normal(e2/l,true,contextptr);
	if (e2.is_symb_of_sommet(at_plus) && e2._SYMBptr->feuille.type==_VECT && e2._SYMBptr->feuille._VECTptr->size()==2)
	  e2=makesequence(e2._SYMBptr->feuille._VECTptr->front(),-e2._SYMBptr->feuille._VECTptr->back());
	else
	  e2=makesequence(e2,0);
	if (is_positive(-l,contextptr)){
	  if (u==at_superieur_strict)
	    return symbolic(at_inferieur_strict,e2);
	  if (u==at_superieur_egal)
	    return symbolic(at_inferieur_egal,e2);
	  if (u==at_inferieur_strict)
	    return symbolic(at_superieur_strict,e2);
	  if (u==at_inferieur_egal)
	    return symbolic(at_superieur_egal,e2);
	  return symbolic(u,e2); // should not happen
	}
	return symbolic(u,e2);
      }
      return symbolic(u,makesequence(e2,0));
    }
    if (ctrl_c || interrupted) { 
      gen res;
      interrupted = true; ctrl_c=false;
      gensizeerr(gettext("Stopped by user interruption."),res);
      return res;
    }
    gen e_copy(e); // was eval(e,1,contextptr));
    if (algnum_normal(e_copy,contextptr))
      return e_copy;
    if (e.type==_VECT)
      return apply(e,contextptr,(const gen_op_context) recursive_normal);
    //recursive BUG F(x):=int(1/sqrt(1+t^2),t,0,x);u(x):=exp(x); F(u(x))
    if (e_copy.is_symb_of_sommet(at_pnt))
      e_copy=e;
    if (e_copy.type==_FRAC)
      return e_copy._FRACptr->normal();
    if (e_copy.type!=_SYMB && e_copy.type!=_MOD)
      return e_copy;
    if (is_inf(e_copy) || is_undef(e_copy) )
      return e_copy;
    vecteur l=lvar(e_copy);
    vecteur l_subst(l);
    iterateur it=l_subst.begin(),itend=l_subst.end();
    for (;it!=itend;++it){
      if (it->type!=_SYMB)
	continue;
      if (it->_SYMBptr->sommet==at_when)
	continue;
      if (it->_SYMBptr->sommet!=at_pow){
	gen tmp=it->_SYMBptr->feuille;
	if (!has_algebraic_program(tmp))
	  tmp=liste2symbolique(symbolique2liste(tmp,contextptr));
	tmp=recursive_normal(tmp,false,contextptr);
	if (is_undef(tmp)) return e;
	*it=it->_SYMBptr->sommet(tmp,contextptr);
	continue;
      }
      if (it->_SYMBptr->feuille._VECTptr->front().type==_INT_ && it->_SYMBptr->feuille._VECTptr->back()==plus_one_half)
	continue;
      vecteur l=lvar(it->_SYMBptr->feuille._VECTptr->back());
      int l_size;
      if (!l.empty() && l.front().type==_VECT)
	l_size=int(l.front()._VECTptr->size());
      else
	l_size=int(l.size());
      gen f,f_num,f_den;
      f=e2r(it->_SYMBptr->feuille._VECTptr->back(),l,contextptr);
      fxnd(f,f_num,f_den);
      gen num=r2sym(f_num,l,contextptr),den=r2sym(f_den,l,contextptr);
      gen base=recursive_normal(it->_SYMBptr->feuille._VECTptr->front(),false,contextptr);
      if ( is_zero(base) || num.type!=_INT_ || den.type!=_INT_ || den.val>MAX_COMMON_ALG_EXT_ORDER_SIZE)
	*it= pow(base,rdiv(num,den,contextptr),contextptr);
      else {
	if (den.val>1 && (is_integer(base) || base.type==_FRAC) && is_positive(base,contextptr)){
	  // should also handle more general base.type 
	  vecteur vtmp(den.val+1);
	  vtmp[0]=1;
	  vtmp.back()=-pow(base,num.val % den.val,contextptr);
	  smaller_factor(vtmp);
	  gen tmp;
	  if (vtmp.size()>2)
	    tmp=r2e(algebraic_EXTension(makevecteur(1,0),vtmp),vecteur(1,vecteur(0)),contextptr);
	  else
	    tmp=-vtmp.back()/vtmp.front();
	  *it=pow(base,num.val/den.val,contextptr)*tmp;
	}
	else {
          // extract from the root before pow
          gen base1=1,base2=1;
          if (den.val==1)
            base2=base;
          else {
            gen fbase=_factors(base,contextptr);
            if (fbase.type==_VECT){
              vecteur v=*fbase._VECTptr;
              for (int i=0;i<v.size();i+=2){
                int mult=v[i+1].val;
                if (mult%den.val==0){
                  if (den.val%2==0 && is_real(v[i],contextptr))
                    v[i]=abs(v[i],contextptr);
                  base1=base1*pow(v[i],mult*num.val/den.val,contextptr);
                }
                else
                  base2=base2*pow(v[i],mult,contextptr);
              }
            }
            else
              base2=base;
          }
          *it= base1*pow(base2,num.val/den.val,contextptr)*pow(base2,rdiv(num.val%den.val,den),contextptr);
        }
      }
    }
    if (l!=l_subst) 
      e_copy=subst(e_copy,l,l_subst,false,contextptr);
    // return global_eval(normal(e_copy),100);
    bool b=calc_mode(contextptr)==1 || abs_calc_mode(contextptr)==38;
    int ca=calc_mode(contextptr);
    calc_mode(0,contextptr);
    calc_mode(0,context0);
    int z=MPZ_MAXLOG2;
    MPZ_MAXLOG2=int(8e7);
    gen res=normal(e_copy,distribute_div,contextptr);
    MPZ_MAXLOG2=z;
    calc_mode(ca,context0);
    calc_mode(ca,contextptr);
    if ( b && !lop(res,at_rootof).empty())
      res=simplifier(ratnormal(normalize_sqrt(e_copy,contextptr),contextptr),contextptr);
    return res;
    // removed eval since it eats neg(x-y)
    // eval(normal(e_copy,distribute_div),contextptr);
  }
  gen recursive_normal(const gen & e,GIAC_CONTEXT){
    gen var,res;
    res=recursive_normal(e,true,contextptr);
    return res;
  }

  gen _recursive_normal(const gen & e,GIAC_CONTEXT){
    if (e.is_symb_of_sommet(at_unit))
      return _usimplify(e,contextptr);
    gen var,res;
    if (is_equal(e))
      return apply_to_equal(e,recursive_normal,contextptr); // symb_equal(_recursive_normal(equal2diff(e),contextptr),0);
    if (is_algebraic_program(e,var,res))
      return symbolic(at_program,makesequence(var,0,recursive_normal(res,contextptr)));
    res=recursive_normal(e,true,contextptr);
    return res;
  }

  gen ratnormal(const gen & e,GIAC_CONTEXT){
    // COUT << e << "\n";
    if (e.type==_VECT)
      return apply(e,ratnormal,contextptr);
    if (e.type==_FRAC){
      gen n=e._FRACptr->num;
      gen d=e._FRACptr->den;
      simplify(n,d);
      if (is_one(d))
	return n;
      if (is_minus_one(d))
	return -n;
      if (is_zero(d)){
	if (is_zero(n))
	  return undef;
	else
	  return unsigned_inf;
      }
      if (is_zero(n))
	return zero;
      return fraction(n,d);
    }
    if (e.type!=_SYMB && e.type!=_MOD)
      return e;
    if (is_inf(e) || is_undef(e) )
      return e;
    matrice l; lvar(e,l);
    if (l.size()>1) l=sort1(l);
    gen fg=e2r(e,l,contextptr); // ok
    if (fg.type==_FRAC && fg._FRACptr->num.type==_FRAC){
      fraction f(fg._FRACptr->num._FRACptr->num,fg._FRACptr->den*fg._FRACptr->num._FRACptr->den);
      f.normal();
      return r2sym(f,l,contextptr); // ok
    }
    if (fg.type==_FRAC && fg._FRACptr->den.type==_CPLX){
      gen tmp=conj(fg._FRACptr->den,contextptr);
      fg._FRACptr->num = fg._FRACptr->num * tmp;
      fg._FRACptr->den = fg._FRACptr->den * tmp;
    }
    return r2sym(fg,l,contextptr);
  }
  gen recursive_ratnormal(const gen & e,GIAC_CONTEXT){
    // COUT << e << "\n";
    if (e.type==_VECT)
      return apply(e,recursive_ratnormal,contextptr);
    if (e.type==_FRAC){
      gen n=e._FRACptr->num;
      gen d=e._FRACptr->den;
      simplify(n,d);
      if (is_one(d))
	return n;
      if (is_minus_one(d))
	return -n;
      if (is_zero(d)){
	if (is_zero(n))
	  return undef;
	else
	  return unsigned_inf;
      }
      if (is_zero(n))
	return zero;
      return fraction(n,d);
    }
    if (e.type!=_SYMB && e.type!=_MOD)
      return e;
    if (is_inf(e) || is_undef(e) )
      return e;
    matrice l; lvar(e,l);
    if (l.size()>1) l=sort1(l);
    gen fg=e2r(e,l,contextptr);
    for (unsigned i=0;i<l.size();++i){
      if (l[i].type==_SYMB)
	l[i]=l[i]._SYMBptr->sommet(recursive_ratnormal(l[i]._SYMBptr->feuille,contextptr),contextptr);
    }
    if (fg.type==_FRAC && fg._FRACptr->num.type==_FRAC){
      fraction f(fg._FRACptr->num._FRACptr->num,fg._FRACptr->den*fg._FRACptr->num._FRACptr->den);
      f.normal();
      return ratnormal(r2sym(f,l,contextptr),contextptr); 
    }
    if (fg.type==_FRAC && fg._FRACptr->den.type==_CPLX){
      gen tmp=conj(fg._FRACptr->den,contextptr);
      fg._FRACptr->num = fg._FRACptr->num * tmp;
      fg._FRACptr->den = fg._FRACptr->den * tmp;
    }
    return ratnormal(r2sym(fg,l,contextptr),contextptr);
  }
  gen rationalgcd(const gen & a, const gen & b,GIAC_CONTEXT){
    gen A,B,C,D;
    if (is_algebraic_program(a,A,B) && is_algebraic_program(b,C,D)){
      if (A==C)
	return symbolic(at_program,makesequence(A,0,gcd(B,D,contextptr)));
      D=subst(D,C,A,false,contextptr);
      return symbolic(at_program,makesequence(A,0,gcd(B,D,contextptr)));
    }
    vecteur l(alg_lvar(a));
    alg_lvar(b,l);
    fraction fa(e2r(a,l,contextptr)),fb(e2r(b,l,contextptr)); // ok
    if (debug_infolevel)
      CERR << "rational gcd begin " << CLOCK() << "\n";
    if (!is_one(fa.den) || !is_one(fb.den))
      CERR << "Warning gcd of fractions " << fa << " " << fb ;
    if (fa.num.type==_FRAC)
      fa.num=fa.num._FRACptr->num;
    if (fb.num.type==_FRAC)
      fb.num=fb.num._FRACptr->num;
    return r2sym(gcd(fa.num,fb.num,contextptr),l,contextptr); // ok
  }

  static gen factor(const polynome & p,const vecteur &l,bool fixed_order,bool with_sqrt,gen divide_an_by,gen & extra_div,GIAC_CONTEXT);

  static gen factor_multivar(const polynome & p,const vecteur &l,bool fixed_order,bool with_sqrt,gen divide_an_by,gen & extra_div,GIAC_CONTEXT){
    polynome pp(p);
    int nvars=int(l.size());
    if (l.front().type==_VECT)
      nvars=int(l.front()._VECTptr->size());
    vector<int> deg(nvars);
    int mindeg=pp.lexsorted_degree();
    int posmin=0;
    for (int i=1;i<nvars;i++){
      int d=pp.degree(i);
      deg[i]=d;
      if (d<mindeg){
	posmin=i;
	mindeg=d;
      }
    }
    // move posmin variable at the beginning for p and l
    vecteur lp;
    if (posmin && !fixed_order){
      vecteur lptemp;
      pp.reorder(transposition(0,posmin,nvars));
      vecteur::const_iterator it;
      if (l.front().type==_VECT){
	it=l.front()._VECTptr->begin();
	++it;
	for (int i=1;i<nvars;i++,++it){
	  if (i==posmin){
	    if (lptemp.empty())
	      lptemp.push_back(*it);
	    else
	      lptemp.insert(lptemp.begin(),*it);
	    lptemp.push_back(l.front()._VECTptr->front());
	  }
	  else
	    lptemp.push_back(*it);
	}
	lp=l;
	lp[0]=lptemp;
      }
      else {
	it=l.begin();
	++it;
	for (int i=1;i<nvars;i++,++it){
	  if (i==posmin){
	    if (lp.empty())
	      lp.push_back(*it);
	    else
	      lp.insert(lp.begin(),*it);
	    lp.push_back(l.front());
	  }
	  else
	    lp.push_back(*it);
	}
      }
    }
    else
      lp=l;
    factorization v;
    polynome p_content(pp.dim);
    if (!factor(pp,p_content,v,false,with_sqrt,complex_mode(contextptr),divide_an_by,extra_div)){
      *logptr(contextptr) << gettext("Not implemented, e.g. for multivariate mod/approx polynomials") << "\n";
      gen tmp;
      if (l.front().type==_VECT)
	tmp=r2sym(p,l.begin(),l.end(),contextptr);
      else
	tmp=r2sym(p,l,contextptr);
      return ratfactor(tmp,with_sqrt,contextptr);
    }
    // factor p_content
    pp=p_content.trunc1();
    vecteur ll;
    if (lp.front().type==_VECT){
      ll=lp;
      ll[0]=cdr_VECT(*(ll[0]._VECTptr));
    }
    else
      ll=cdr_VECT(lp);
    gen deno=1; lcmdeno(pp,deno); pp=deno*pp; deno=deno*extra_div; extra_div=1;
    gen tmp=factor(pp,ll,false,with_sqrt,1,extra_div,contextptr);
    tmp=tmp*r2sym(v,lp,contextptr)/r2sym(extra_div,ll,contextptr)/r2sym(deno,ll,contextptr);
    extra_div=1;
    return tmp;
  }

  static gen factor(const polynome & p,const vecteur &l,bool fixed_order,bool with_sqrt,gen divide_an_by,gen & extra_div,GIAC_CONTEXT){
    // find main var, make permutation on f.num, f.den and l
    //    COUT << "Factor " << p << " " << l << "\n";
    if (is_one(p))
      return 1;
    if (l.empty() )
      return r2sym(p,l,contextptr);
    if (!p.dim){
      gen tmp;
      if (l.front().type==_VECT)
	tmp=r2sym(p,l.begin(),l.end(),contextptr);
      else
	tmp=r2sym(p,l,contextptr);
      return ratfactor(tmp,with_sqrt,contextptr);
    }
    if (p.dim>1)
      return factor_multivar(p,l,fixed_order,with_sqrt,divide_an_by,extra_div,contextptr);
    factorization v;
    polynome p_content(p.dim);
    if (!factor(p,p_content,v,false,with_sqrt,complex_mode(contextptr),divide_an_by,extra_div)){
      *logptr(contextptr) << gettext("Not implemented, e.g. for multivariate mod/approx polynomials") << "\n";
      gen tmp;
      if (l.front().type==_VECT)
	tmp=r2sym(p,l.begin(),l.end(),contextptr);
      else
	tmp=r2sym(p,l,contextptr);
      return ratfactor(tmp,with_sqrt,contextptr);
    }
    // factor p_content
    if (is_one(p_content))
      return r2sym(v,l,contextptr)/r2sym(extra_div,l,contextptr);
    else {
      gen tmp(p_content);
      simplify(tmp,extra_div);
      if ( (tmp.type>_POLY || (tmp.type==_POLY && !tmp._POLYptr->coord.empty() && tmp._POLYptr->coord.front().value.type==_USER)) && 
	   !v.empty() && v.front().mult==1 && (v.size()==1 || v.front().fact.degree(0)==0)
	   ){ // for GF factorization
	v.front().fact = tmp.type==_POLY?(*tmp._POLYptr*v.front().fact):(tmp*v.front().fact);
	return r2sym(v,l,contextptr)/r2sym(extra_div,l,contextptr);
      }
      polynome pp=p_content.trunc1();
      vecteur ll;
      if (l.front().type==_VECT){
	ll=l;
	ll[0]=cdr_VECT(*(ll[0]._VECTptr));
      }
      else
	ll=cdr_VECT(l);
      tmp=factor(pp,ll,false,with_sqrt,1,extra_div,contextptr); //cerr << v << "\n";
      return tmp*r2sym(v,l,contextptr)/r2sym(extra_div,l,contextptr);
      // return r2sym(tmp,l,contextptr)*r2sym(v,l,contextptr)/r2sym(extra_div,l,contextptr);
    }
  }

  static gen var_factor(const gen & e,const vecteur & l,bool fixed_order,bool with_sqrt,const gen & divide_an_by,GIAC_CONTEXT){
    if (e.type!=_POLY)
      return r2sym(e/divide_an_by,l,contextptr);
    gen extra_div=1;
    return factor(*e._POLYptr,l,fixed_order,with_sqrt,divide_an_by,extra_div,contextptr);
  }

  gen ordered_factor(const gen & ee,vecteur & l,bool with_sqrt,GIAC_CONTEXT){
    gen e=normalize_sqrt(ee,contextptr);
    alg_lvar(e,l);
    gen f_num,f_den,f;
    f=e2r(e,l,contextptr);
    fxnd(f,f_num,f_den);
    return rdiv(var_factor(f_num,l,true,with_sqrt,1,contextptr),var_factor(f_den,l,true,with_sqrt,1,contextptr));
  }

  gen factor(const gen & e,const identificateur & x,bool with_sqrt,GIAC_CONTEXT){
    if (e.type==_VECT){
      vecteur w;
      vecteur::const_iterator it=e._VECTptr->begin(),itend=e._VECTptr->end();
      for (;it!=itend;++it)
	w.push_back(factor(*it,x,with_sqrt,contextptr));
      return w;
    }
    vecteur l(1,vecteur(1,x)); // insure x is the main var
    return ordered_factor(e,l,with_sqrt,contextptr);
  }

  static gen in_factor(const gen & e_,bool with_sqrt,const gen & divide_an_by,GIAC_CONTEXT){
    gen e(normalize_sqrt(e_,contextptr));
    if (e.type==_VECT){
      vecteur w;
      vecteur::const_iterator it=e._VECTptr->begin(),itend=e._VECTptr->end();
      for (;it!=itend;++it)
	w.push_back(in_factor(*it,with_sqrt,divide_an_by,contextptr));
      return w;
    }
    vecteur l;
    alg_lvar(e,l);
    if (!l.empty() && l.front().type==_VECT && l.front()._VECTptr->empty())
      l=lvar(e);
#ifdef POCKETCAS
    if (l.size()==1 && contains(l.front(),vx_var)){ // x first
      l=vecteur(1,vecteur(1,vx_var));
      alg_lvar(e,l);
    }
#endif
    /* if (calc_mode(contextptr)==1 && l.size()==1 && l.front().type==_VECT){
      sort(l.front()._VECTptr->begin(),l.front()._VECTptr->end(),islesscomplexthanf);
      } */
    gen f_num,f_den,f,dnum,dden(1);
    f=e2r(e,l,contextptr);
    fxnd(f,f_num,f_den);
    if (has_EXT(f_num))
      simplify3(f_num,f_den);
    if (divide_an_by.type>=_IDNT){
      gen divide=e2r(divide_an_by,l,contextptr);
      fxnd(divide,dnum,dden);
      if (dnum.type==_POLY){
	if (!Tis_constant(*dnum._POLYptr)){
	  simplify3(f_num,dnum);
	}
	if (dnum.type==_POLY)
	  dnum=dnum._POLYptr->coord.front().value;
      }
      if (dden.type==_POLY)
	dden=dden._POLYptr->coord.front().value;
    }
    else {
      dnum=divide_an_by;
    }
    if (dden.type==_CPLX){
      gen tmp=conj(dden,contextptr);
      dnum=dnum*tmp;
      dden=dden*tmp;
      f_num=f_num*tmp;
      f_den=f_den*tmp;
    }
    if (dnum==-1){
      f_num=-f_num;
      dnum=1;
    }
    gen N=var_factor(f_num,l,false,with_sqrt,dnum,contextptr);
    gen D=var_factor(f_den,l,false,with_sqrt,dden,contextptr);
    return rdiv(N,D);
  }

  gen factor(const gen & ee,bool with_sqrt,const gen & divide_an_by,GIAC_CONTEXT){
    if (xcas_mode(contextptr)==3 && is_integer(ee))
      return _ifactor(ee,contextptr);
    gen e(ee);
    if (has_num_coeff(ee))
      e=e.evalf(1,contextptr);
    else {
      vecteur L=lop_pow(e);
      L=lidnt(L);
      // make cfactor(-x/4*pi^2+i*sqrt(3+x)/2+x^2+3/4) work
      e=in_factor(e,with_sqrt && L.empty(),divide_an_by,contextptr);
      return e;
    }
    return in_factor(e,with_sqrt,divide_an_by,contextptr);
  }

  gen factor(const gen & ee,bool with_sqrt,GIAC_CONTEXT){
    return factor(ee,with_sqrt,plus_one,contextptr);
  }

  gen ratfactor(const gen & ee,bool with_sqrt,GIAC_CONTEXT){
    if (has_num_coeff(ee))
      return ee;
    gen e(normalize_sqrt(ee,contextptr));
    if (has_num_coeff(ee))
      e=e.evalf(1,contextptr);
    if (e.type==_VECT){
      vecteur w;
      vecteur::const_iterator it=e._VECTptr->begin(),itend=e._VECTptr->end();
      for (;it!=itend;++it)
	w.push_back(ratfactor(*it,with_sqrt,contextptr));
      return w;
    }
    vecteur l;
    lvar(e,l);
    gen f_num,f_den,f;
    f=e2r(e,l,contextptr);
    fxnd(f,f_num,f_den);
    if (with_sqrt)
      l=vecteur(1,l);
    gen tmp=rdiv(var_factor(f_num,l,false,with_sqrt,1,contextptr),var_factor(f_den,l,false,with_sqrt,1,contextptr));
    return tmp;
  }

  gen factor(const gen & ee,const gen & f,bool with_sqrt,GIAC_CONTEXT){
    if (ee.type==_VECT){
      vecteur & v=*ee._VECTptr;
      int s=int(v.size());
      vecteur res(s);
      for (int i=0;i<s;++i)
	res[i]=factor(v[i],f,with_sqrt,contextptr);
      return res;
    }
    gen e(ee);
    if (has_num_coeff(ee))
      e=e.evalf(1,contextptr);
    if (f.type==_IDNT)
      return factor(e,*f._IDNTptr,with_sqrt,contextptr);
    if (f.type==_VECT){
      // should check that *f._VECTptr is made only of atomic _SYMB
      return ordered_factor(e,*f._VECTptr,with_sqrt,contextptr);
    }
    return gentypeerr(contextptr);
  }

  gen factorcollect(const gen & args,bool with_sqrt,GIAC_CONTEXT){
    if (args.type!=_VECT)
      return factor(args,with_sqrt,contextptr);
    vecteur & v=*args._VECTptr;
    if (v.empty())
      return gensizeerr(contextptr);
    if (v.size()==1)
      return vecteur(1,factor(v.front(),with_sqrt,contextptr));
    if (args.subtype==_SEQ__VECT){
      if (v.size()>2)
	toomanyargs("factor");
      if (v.back()==at_sqrt)
	return factor(v.front(),true,contextptr);
      if (v.back().type!=_IDNT){ // FIXME could be improved!
	gen f=v.back();
	if (v.back().type==_VECT)
	  f=symbolic(at_prod,v.back());
	gen res=factor(v.front()*f,with_sqrt,f,contextptr);
	return res;
      }
      return factor(v.front(),v.back(),with_sqrt,contextptr);
    }
    int s=int(v.size());
    vecteur res(s);
    for (int i=0;i<s;++i)
      res[i]=factor(v[i],with_sqrt,contextptr);
    return res;
  }
  gen partfrac(const gen & e_,const vecteur & l,bool with_sqrt,GIAC_CONTEXT){
    gen ext(1),e(e_);
    if (e.type==_VECT && e.subtype==_SEQ__VECT && e._VECTptr->size()==2){
      ext=e._VECTptr->back();
      e=e._VECTptr->front();
    }
    if (e.type==_VECT){
      vecteur w;
      vecteur::const_iterator it=e._VECTptr->begin(),itend=e._VECTptr->end();
      for (;it!=itend;++it)
	w.push_back(partfrac(*it,l,with_sqrt,contextptr));
      return w;
    }
    int l_size;
    gen xvar;
    if (!l.empty() && l.front().type==_VECT){
      l_size=int(l.front()._VECTptr->size());
      xvar=l.front();
    }
    else {
      l_size=int(l.size());
      xvar=l;
    }
    if (!l_size)
      return e;
    else
      xvar=xvar._VECTptr->front();
    gen r=e2r(e,l,contextptr);
    gen r_num,r_den;
    fxnd(r,r_num,r_den);
    if (r_den.type!=_POLY){
      if (r_num.type==_POLY)
	return rdiv(r2sym(r_num,l,contextptr),r2sym(r_den,l,contextptr));
      else 
	return e;
    }
    polynome f_den(*r_den._POLYptr),f_num(l_size);
    if (r_num.type==_POLY)
      f_num=*r_num._POLYptr;
    else
      f_num=polynome(r_num,l_size);
    factorization vden;
    polynome p_content(l_size);
    gen extra_div=1;
    if (ext!=1){
      ext=e2r(ext,vecteur(1,vecteur(0)),contextptr);
      if (ext.type!=_EXT)
	ext=1; 
    }
    factor(ext*f_den,p_content,vden,false,with_sqrt,complex_mode(contextptr),ext,extra_div);
    vector< pf<gen> > pfde_VECT;
    polynome ipnum(l_size),ipden(l_size);
    partfrac(f_num,f_den,vden,pfde_VECT,ipnum,ipden,!with_sqrt);
    gen res=rdiv(r2sym(gen(ipnum),l,contextptr),r2sym(gen(ipden),l,contextptr));
    vector< pf<gen> > ::const_iterator it=pfde_VECT.begin(),itend=pfde_VECT.end();
    for (;it!=itend;++it){
      const pf<gen> & current=*it;
      gen reste(r2e(gen(current.num),l,contextptr)),deno(r2e(gen(current.fact),l,contextptr));
      polynome p=it->mult==1?it->fact:pow(it->fact,it->mult),quo,rem;
      it->den.TDivRem(p,quo,rem,true);
      gen cur_deno(r2e(quo,l,contextptr));
      if (current.mult==1){
	  // unitarize
	  gen tmp(_lcoeff(makesequence(deno,xvar),contextptr));
	  reste=ratnormal(reste/tmp/cur_deno,contextptr);
	  deno=recursive_normal(deno/tmp,contextptr);
	res += reste/deno;
      }
      else {
	for (int i=0;i<current.mult;++i){
	  gen tmp(_quorem(makesequence(reste,deno,xvar),contextptr));
	  if (tmp.type!=_VECT)
	    return gensizeerr(contextptr);
	  vecteur & vtmp=*tmp._VECTptr;
	  reste=vtmp.front();
	  res=res+normal(vtmp.back()/cur_deno,contextptr)/pow(deno,current.mult-i);
	}
      }
    }
    return res; // +r2sym(pfde_VECT,l);
  }

  gen partfrac(const gen & e_,const identificateur & x,bool with_sqrt,GIAC_CONTEXT){
    gen e=normalize_sqrt(e_,contextptr);
    vecteur l;
    l.push_back(x); // insure x is the main var
    l=vecteur(1,l);
    alg_lvar(e,l);
    return partfrac(e,l,with_sqrt,contextptr);
  }

  gen partfrac(const gen & e_,bool with_sqrt,GIAC_CONTEXT){
    gen e=normalize_sqrt(e_,contextptr);
    vecteur l;
    alg_lvar(e,l);
    if (!l.empty() && l.front().type==_VECT && l.front()._VECTptr->empty())
      return e_;
    if (l.size()==1 && contains(l.front(),vx_var)){ // x first
      l=vecteur(1,vecteur(1,vx_var));
      alg_lvar(e,l);
    }
    return partfrac(e,l,with_sqrt,contextptr);
  }

  gen partfrac(const gen & e,const gen & f,bool with_sqrt,GIAC_CONTEXT){
    if (f.type==_IDNT)
      return partfrac(e,*f._IDNTptr,with_sqrt,contextptr);
    if (f.type==_VECT){
      // should check that *f._VECTptr is made only of atomic _SYMB
      return partfrac(e,*f._VECTptr,with_sqrt,contextptr);
    }
    if (f.type==_SYMB)
      return partfrac(makesequence(e,f),with_sqrt,contextptr);
    return gentypeerr(contextptr);
  }

  /* User functions */
  static gen symb2poly(const gen & fr,const gen & ba,GIAC_CONTEXT){
    if (fr.type==_VECT)
      return apply1st(fr,ba,contextptr,symb2poly);
    if (ba.type==_VECT)
      return e2r(fr,*(ba._VECTptr),contextptr);
    if (ba.type!=_IDNT && ba.type!=_SYMB)
      return gensizeerr(contextptr);
    vecteur l(1,ba);
    lvar(fr,l);
    gen temp=e2r(fr,l,contextptr);
    l.erase(l.begin());
    gen res;
    gen tmp2(polynome2poly1(temp,1));
    //res=l.empty()?tmp2:r2e(tmp2,l,contextptr); 
    res=l.empty()?tmp2:((tmp2.type==_FRAC && tmp2._FRACptr->den.type==_VECT && tmp2._FRACptr->den._VECTptr->size()>1)?gen(fraction(r2e(tmp2._FRACptr->num,l,contextptr),r2e(tmp2._FRACptr->den,l,contextptr))):r2e(tmp2,l,contextptr));
    if (res.type==_FRAC && res._FRACptr->num.type==_VECT && res._FRACptr->den.type<_POLY){
      res=inv(res._FRACptr->den,contextptr)*res._FRACptr->num;
    }
    if (res.type==_VECT && calc_mode(contextptr)!=1)
      res.subtype=_POLY1__VECT;
    return res;
  }
  gen _e2r(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return _e2r(makesequence(args,vx_var),contextptr);
    vecteur & v=*args._VECTptr;
    int s=int(v.size());
    if (s<2)
      return gendimerr(contextptr);
    gen res=v.front();
    if (s==2 && v[1].type==_VECT)
      return symb2poly(res,v[1],contextptr);
    for (int i=1;i<s;++i){
      res=symb2poly(res,v[i],contextptr);
    }
    if (res.type!=_VECT && res.type!=_FRAC && res.type!=_POLY) 
      return gen(vecteur(1,res),calc_mode(contextptr)!=1?_POLY1__VECT:0);
    else
      return res;
  }

  static const char _e2r_s []="e2r";
  symbolic symb_e2r(const gen & arg1,const gen & arg2){
    return symbolic(at_e2r,makesequence(arg1,arg2));
  }
  static define_unary_function_eval (__e2r,&_e2r,_e2r_s);
  define_unary_function_ptr5( at_e2r ,alias_at_e2r,&__e2r,0,true);

  gen _r2e(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type!=_VECT || args.subtype!=_SEQ__VECT)
      return _r2e(gen(makevecteur(args,vx_var),_SEQ__VECT),contextptr);
    vecteur & v=*args._VECTptr;
    int s=int(v.size());
    if (s<2)
      return _r2e(gen(makevecteur(args,vx_var),_SEQ__VECT),contextptr);
    gen res=v[0];
    for (int i=1;i<s;++i){
      if (v[i].type==_VECT)
	res=r2e(res,*v[i]._VECTptr,contextptr);
      else {
	if (res.type==_VECT)
	  res=horner(*res._VECTptr,v[i]);
      }
    }
    return res;
  }

  static const char _r2e_s []="r2e";
  symbolic symb_r2e(const gen & arg1,const gen & arg2){
    return symbolic(at_r2e,makesequence(arg1,arg2));
  }
  static define_unary_function_eval (__r2e,&_r2e,_r2e_s);
  define_unary_function_ptr5( at_r2e ,alias_at_r2e,&__r2e,0,true);

  static const char _normal_s []="normal";
  symbolic symb_normal(const gen & args){
    return symbolic(at_normal,args);
  }
  static define_unary_function_eval (__normal,(const gen_op_context)_recursive_normal,_normal_s);
  define_unary_function_ptr5( at_normal ,alias_at_normal,&__normal,0,true);

  static const char _non_recursive_normal_s []="non_recursive_normal";
  symbolic symb_non_recursive_normal(const gen & args){
    return symbolic(at_non_recursive_normal,args);
  }
  static define_unary_function_eval (__non_recursive_normal,(const gen_op_context)normal,_non_recursive_normal_s);
  define_unary_function_ptr5( at_non_recursive_normal ,alias_at_non_recursive_normal,&__non_recursive_normal,0,true);


  static const char _factor_s []="factor";
  symbolic symb_factor(const gen & args){
    return symbolic(at_factor,args);
  }
  gen _factor(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (is_integer(args)){
#if defined KHICAS || defined SDL_KHICAS
      return _ifactor(args,contextptr);
#else
      *logptr(contextptr) << "Run ifactor(" << args << ") for integer factorization." << "\n";
      return args;
#endif
    }
    if (args.is_symb_of_sommet(at_unit))
      return mksa_reduce(args,contextptr);
    if (is_equal(args))
      return apply_to_equal(args,_factor,contextptr);
    if (args.type==_VECT && args._VECTptr->size()==2 && is_equal(args._VECTptr->front())){
      gen x=args._VECTptr->back();
      gen a=_left(args._VECTptr->front(),contextptr);
      gen b=_right(args._VECTptr->front(),contextptr);
      return symb_equal(_factor(makesequence(a,x),contextptr),_factor(makesequence(b,x),contextptr));
    }
    gen var,res;
    if (args.type!=_VECT && is_algebraic_program(args,var,res))
      return symbolic(at_program,makesequence(var,0,_factor(res,contextptr)));
    if (xcas_mode(contextptr)==3)
      res=factorcollect(args,lvar(args).size()==1,contextptr);
    else
      res=factorcollect(args,withsqrt(contextptr),contextptr);
    return res;
  }
  static define_unary_function_eval (__factor,&_factor,_factor_s);
  define_unary_function_ptr5( at_factor ,alias_at_factor,&__factor,0,true);

  static const char _collect_s []="collect";
  gen _collect(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    gen var,res;
    if (is_algebraic_program(args,var,res))
      return symbolic(at_program,makesequence(var,0,_collect(res,contextptr)));
    if (is_equal(args))
      return apply_to_equal(args,_collect,contextptr);
    if (args.type==_VECT && args.subtype==_SEQ__VECT && args._VECTptr->size()>=2&& args._VECTptr->front().type!=_VECT){
      vecteur v(args._VECTptr->begin()+1,args._VECTptr->end());
      res=_symb2poly(args,contextptr);
      if (res.type!=_FRAC){
	res=_poly2symb(gen(mergevecteur(vecteur(1,res),v),_SEQ__VECT),contextptr);
	return res;
      }
    }
    res=factorcollect(args,false,contextptr);
    return res;
  }
  static define_unary_function_eval (__collect,&_collect,_collect_s);
  define_unary_function_ptr5( at_collect ,alias_at_collect,&__collect,0,true);

  static const char _partfrac_s []="partfrac";
  symbolic symb_partfrac(const gen & args){
    return symbolic(at_partfrac,args);
  }
  gen _partfrac(const gen & args_,GIAC_CONTEXT){
    if (args_.type==_STRNG && args_.subtype==-1) return  args_;
    gen args(args_),var,res;
    if (is_algebraic_program(args,var,res))
      return symbolic(at_program,makesequence(var,0,_partfrac(res,contextptr)));
    if (is_equal(args))
      return apply_to_equal(args,_partfrac,contextptr);
    args=exact(args,contextptr);
    if (args.type!=_VECT)
      return partfrac(args,withsqrt(contextptr),contextptr);
    if (args._VECTptr->size()>2)
      return gentoomanyargs(_partfrac_s);
    return partfrac(args._VECTptr->front(),args._VECTptr->back(),withsqrt(contextptr),contextptr);
  }
  static define_unary_function_eval (__partfrac,&_partfrac,_partfrac_s);
  define_unary_function_ptr5( at_partfrac ,alias_at_partfrac,&__partfrac,0,true);

  static const char _resultant_s []="resultant";
  symbolic symb_resultant(const gen & args){
    return symbolic(at_resultant,args);
  }
  gen _resultant(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return gentypeerr(contextptr);
    vecteur v =*args._VECTptr;
    int s=int(v.size());
    if (s<2)
      toofewargs(_resultant_s);
    if (has_num_coeff(v)){
      gen g=exact(args,contextptr);
      if (!has_num_coeff(g)){
	g=_resultant(g,contextptr);
	return evalf(g,1,contextptr);
      }
    }
    if (v[0].type==_VECT && v[1].type==_VECT){ 
#ifdef FXCG
	gen res;
	subresultant(*v[0]._VECTptr,*v[1]._VECTptr,res);
	return res;
#else
      gen g1,g2;
      int t1=coefftype(*v[0]._VECTptr,g1),t2=coefftype(*v[1]._VECTptr,g2);
      double eps=epsilon(contextptr);
      if (t1==0 && t2==0){
	if (eps>0)
	  return mod_resultant(*v[0]._VECTptr,*v[1]._VECTptr,eps);
	gen res;
	subresultant(*v[0]._VECTptr,*v[1]._VECTptr,res);
	return res;
      }
      if (t1==_MOD || t2==_MOD){
	gen m=t1==_MOD?*(g1._MODptr+1):*(g2._MODptr+1);
	modpoly A=unmod(*v[0]._VECTptr,m);
	modpoly B=unmod(*v[1]._VECTptr,m);
	gen res;
	if (ntlresultant(A,B,m,res))
	  return makemod(res,m);
	if (m.type==_INT_){
	  vector<int> a,b,tmp1,tmp2;
	  vecteur2vector_int(A,m.val,a);
	  vecteur2vector_int(B,m.val,b);
	  return makemod(resultant_int(a,b,tmp1,tmp2,m.val),m);
	}
#if defined INT128 && !defined USE_GMP_REPLACEMENTS && !defined BF2GMP_H 
	if (m.type==_ZINT && sizeinbase2(m)<61){
	  longlong p=mpz_get_si(*m._ZINTptr);
	  int n=giacmax(A.size(),B.size());
	  int l=sizeinbase2(n);
	  if (((p-1)>>l)<<l==p-1){
	    vector<longlong> a,b,tmp1,tmp2;
	    vecteur2vector_ll(A,p,a);
	    vecteur2vector_ll(B,p,b);
	    return makemod(resultantll(a,b,tmp1,tmp2,p),m);
	  }
	}
#endif
	return makemod(mod_resultant(A,B,eps),m);
      }
#endif // FXCG
      // not very efficient...
      gen g(identificateur("tresultant"));
      v[0]=_poly2symb(makesequence(v[0],g),contextptr);
      v[1]=_poly2symb(makesequence(v[1],g),contextptr);
      v.insert(v.begin()+2,g);
      s++;
    }
    if (s==2){
      if (v[0].type==_POLY && v[1].type==_POLY)
	return resultant(*v[0]._POLYptr,*v[1]._POLYptr);
      v.push_back(vx_var);
    }
    if (has_num_coeff(v))
      return _det(_sylvester(args,contextptr),contextptr); 
    if (v.back()==at_sylvester || v.back()==at_det)
      return _det(_sylvester(gen(vecteur(args._VECTptr->begin(),args._VECTptr->begin()+s-1),_SEQ__VECT),contextptr),contextptr);
    if (v.back()==at_lagrange && s>=3){
      if (debug_infolevel)
	CERR << CLOCK()*1e-6 << " interp resultant begin " << "\n";
      gen p=v[0];
      gen q=v[1];
      gen x=vx_var;
      if (s>3)
	x=v[2];
      gen dbg; // dbg=_resultant(makesequence(p,q,x),contextptr);
      vecteur w(1,x);
      lvar(p,w);
      lvar(q,w);
      int m=_degree(makesequence(p,x),contextptr).val;
      int n=_degree(makesequence(q,x),contextptr).val;
      if (w.size()==1 && m+n>GIAC_PADIC){
	gen S=_sylvester(makesequence(p,q,x),contextptr);
	return _det(S,contextptr);
      }
      if (w.size()>1){
	gen y=w[1];
	vecteur rargs(3,x);
	if (s>4 && equalposcomp(w,v[3])){
	  y=v[3];
	  if (s>5){
	    for (int i=4;i<s-1;++i)
	      rargs.push_back(v[i]);
	    rargs.push_back(at_lagrange);
	  }
	  else {
	    if (m+n>=GIAC_PADIC)
	      rargs.push_back(at_lagrange);
	  }
	}
	else {
	  if (m+n>=GIAC_PADIC)
	    rargs.push_back(at_lagrange);
	}
	if (v[s-2]==at_det){
	  gen S=_sylvester(makesequence(p,q,x),contextptr);
	  return _det(gen(makevecteur(S,at_lagrange),_SEQ__VECT),contextptr);
	}
	// bound on degree of resultant with respect to y
	int a=_total_degree(makesequence(p,makevecteur(x,y)),contextptr).val;
	int b=_total_degree(makesequence(q,makevecteur(x,y)),contextptr).val;
	// first estimate n*(a-m)+m*b 
	int d1=n*(a-m)+m*b;
	// second estimate 
	a=_degree(makesequence(p,y),contextptr).val;
	b=_degree(makesequence(q,y),contextptr).val;
	// a*n+b*m
	int d2=a*n+b*m;
	int d=giacmin(d1,d2);
	if (debug_infolevel)
	  CERR << CLOCK()*1e-6 << " interp degree " << d << "\n";	
	vecteur X(d+1),Y(d+1);
	int j=-d/2;
	gen pl=_lcoeff(makesequence(p,x),contextptr);
	gen ql=_lcoeff(makesequence(q,x),contextptr);
	if (rargs.back()!=at_lagrange || w.size()==2){
	  // prepare p and q in polynomial format
	  vecteur l;
	  l.push_back(y);
	  l.push_back(x);
	  l=vecteur(1,l);
	  alg_lvar(p,l);
	  alg_lvar(q,l);
	  gen f1,f1_num,f1_den,f2,f2_num,f2_den;
	  f1=e2r(makevecteur(p,q),l,contextptr);
	  f2=f1[1];
	  f1=f1[0];
	  fxnd(f1,f1_num,f1_den);
	  fxnd(f2,f2_num,f2_den);
	  if ( (f1_num.type==_POLY) && (f2_num.type==_POLY)){
	    const polynome & pp=*f1_num._POLYptr;
	    const polynome & qp=*f2_num._POLYptr;
	    gen coeff;
	    if (!interpolable_resultant(pp,d,coeff,true,contextptr) || !interpolable_resultant(qp,d,coeff,true,contextptr))
	      return gensizeerr(gettext("Not enough elements in field to interpolate. Try in a field extension"));
	    if (coeff.type==_USER) j=0;
	    int dim=pp.dim;
	    vecteur vp,vq,vp0,vq0;
	    polynome2poly1(pp,1,vp);
	    polynome2poly1(qp,1,vq);
	    polynome pp0(pp);
	    pp0.reorder(transposition(0,1,dim));
	    pp0=firstcoeff(pp0).trunc1();
	    polynome qp0(qp);
	    qp0.reorder(transposition(0,1,dim));
	    qp0=firstcoeff(qp0).trunc1();
	    polynome2poly1(pp0,1,vp0);
	    polynome2poly1(qp0,1,vq0);
	    gen den=pow(r2sym(f1_den,l,contextptr),qp.lexsorted_degree(),contextptr)*pow(r2sym(f2_den,l,contextptr),pp.lexsorted_degree(),contextptr);
	    vecteur tmpv1,tmpv2,S;
	    for (int i=0;i<=d;++i,++j){
	      if (debug_infolevel && i%16==0)
		CERR << CLOCK()*1e-6 << " interp horner " << i << "\n";
	      gen xi;
	      for (;;++j){
		// find evaluation preserving degree in x
		if (0 && j==0)
		  CERR << "j" << "\n";
		xi=interpolate_xi(j,coeff);
		gen hp=horner(vp0,xi);
		gen hq=horner(vq0,xi);
		if (!is_zero(hp) && !is_zero(hq)){
		  break;
		}
	      }
	      X[i]=xi;
	      gen gp=horner(vp,xi);
	      gen gq=horner(vq,xi);
	      if (debug_infolevel && i%16==0)
		CERR << CLOCK()*1e-6 << " interp resultant " << j << ", " << 100*double(i)/(d+1) << "% done" << "\n";
	      if (gp.type==_POLY && gq.type==_POLY){
		if (0 &&
		    m>=GIAC_PADIC/2 && n>=GIAC_PADIC/2
		    )
		  resultant_sylvester(*gp._POLYptr,*gq._POLYptr,tmpv1,tmpv2,S,Y[i]);
		else
		  Y[i]=resultant(*gp._POLYptr,*gq._POLYptr);
		continue;
	      }
	      if (gp.type==_POLY){
		Y[i]=pow(gq,gp._POLYptr->lexsorted_degree(),contextptr);
		continue;
	      }
	      if (gq.type==_POLY){
		Y[i]=pow(gp,gq._POLYptr->lexsorted_degree(),contextptr);
		continue;		
	      }
	      Y[i]=1;
	    }
	    if (debug_infolevel)
	      CERR << CLOCK()*1e-6 << " interp dd " << "\n";
	    vecteur R=divided_differences(X,Y);
	    if (debug_infolevel)
	      CERR << CLOCK()*1e-6 << " interp build " << "\n";
	    modpoly resp(1,R[d]),tmp; // cst in y
	    for (int i=d-1;i>=0;--i){
	      operator_times(resp,makevecteur(1,-X[i]),0,tmp);
	      if (tmp.empty())
		tmp=vecteur(1,R[i]);
	      else
		tmp.back() += R[i];
	      tmp.swap(resp);
	    }
	    resp=trim(resp,0);
	    vecteur & lf=*l.front()._VECTptr;
	    lf.erase(lf.begin()); // remove y
	    if (debug_infolevel)
	      CERR << CLOCK()*1e-6 << " interp convert " << "\n";
	    gen resg=r2sym(resp,l,contextptr);
	    if (resg.type==_VECT)
	      resg=symb_horner(*resg._VECTptr,y);
	    return resg/den;	    
	  }
	}
	for (int i=0;i<=d;++i,++j){
	  if (debug_infolevel)
	    CERR << CLOCK()*1e-6 << " interp resultant " << i << "\n";
	  for (;;++j){
	    // find evaluation preserving degree in x
	    if (!is_zero(subst(pl,y,j,false,contextptr))&& !is_zero(subst(ql,y,j,false,contextptr)))
	      break;
	  }
	  gen py=subst(p,y,j,false,contextptr);
	  gen qy=subst(q,y,j,false,contextptr);
	  X[i]=j;
	  rargs[0]=py;
	  rargs[1]=qy;
	  Y[i]=_resultant(gen(rargs,_SEQ__VECT),contextptr);
	  if (0){
	    gen dbgtst=subst(dbg,y,j,false,contextptr);
	    if (!is_zero(ratnormal(dbgtst-Y[i],contextptr)))
	      CERR << "err" << "\n";
	  }
	}
	if (debug_infolevel)
	  CERR << CLOCK()*1e-6 << " interp interpolation begin " << "\n";
	gen r=_lagrange(makesequence(X,Y,y),contextptr);
	if (debug_infolevel)
	  CERR << CLOCK()*1e-6 << " interp interpolation end " << "\n";
	return r;
      }
    }
    if (v.size()>3)
      return gentoomanyargs(_resultant_s);
    if (v.back().type==_MOD)
      v.back()=*v.back()._MODptr;
    if (v.back().is_symb_of_sommet(at_prod)){
      const gen & f = v.back()._SYMBptr->feuille;
      if (f.type==_VECT && f._VECTptr->size()==2 && f._VECTptr->front().type==_MOD)
	v.back()=f._VECTptr->back();
    }
    gen x=v.back();
    vecteur l;
    l.push_back(x);
    l=vecteur(1,l);
    gen p1=v.front(),p2=v[1];
    alg_lvar(p1,l);
    alg_lvar(p2,l);
    int l_size;
    if (!l.empty() && l.front().type==_VECT)
      l_size=int(l.front()._VECTptr->size());
    else
      l_size=int(l.size());
    gen f1,f1_num,f1_den,f2,f2_num,f2_den;
    f1=e2r(makevecteur(p1,p2),l,contextptr);
    f2=f1[1];
    f1=f1[0];
    fxnd(f1,f1_num,f1_den);
    fxnd(f2,f2_num,f2_den);
    if ( (f1_num.type==_POLY) && (f2_num.type==_POLY))
      return r2sym(gen(resultant(*f1_num._POLYptr,*f2_num._POLYptr)),l,contextptr)/pow(r2sym(f1_den,l,contextptr),f2_num._POLYptr->lexsorted_degree(),contextptr)/pow(r2sym(f2_den,l,contextptr),f1_num._POLYptr->lexsorted_degree(),contextptr);
    if (is_zero(f1))
      return f1;
    if (is_zero(f2))
      return f2;
    return 1;
  }
  static define_unary_function_eval (__resultant,&_resultant,_resultant_s);
  define_unary_function_ptr5( at_resultant ,alias_at_resultant,&__resultant,0,true);

  static const char _discriminant_s []="discriminant";
  gen _discriminant(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    vecteur v =gen2vecteur(args);
    int s=int(v.size());
    if (s==0)
      toofewargs("discriminant");
    if (has_num_coeff(v)){
      gen g=exact(args,contextptr);
      if (!has_num_coeff(g)){
	g=_discriminant(g,contextptr);
	return evalf(g,1,contextptr);
      }
    }
    if (v[0].type==_VECT ){
      int n=v[0]._VECTptr->size()-1;
      if (n<0) return undef;
      gen r=_resultant(makesequence(v[0],derivative(*v[0]._VECTptr)),contextptr)/v[0]._VECTptr->front();
      r=ratnormal(r,contextptr);
      if ((n*(n-1))/2 % 2)
        return -r;
      else
        return r;
    }
    if (v.size()==1)
      v.push_back(vx_var);
    if (v.size()>2)
      return gentoomanyargs("discriminant");
    gen x=v.back();
    gen p1=v.front(),p2=derive(p1,x,contextptr);
    gen r=_resultant(makesequence(p1,p2,x),contextptr)/_lcoeff(makesequence(p1,x),contextptr);
    r=ratnormal(r,contextptr);
    int n=_degree(makesequence(p1,x),contextptr).val;
    if ((n*(n-1))/2 % 2)
      return -r;
    else
      return r;
  }
  static define_unary_function_eval (__discriminant,&_discriminant,_discriminant_s);
  define_unary_function_ptr5( at_discriminant ,alias_at_discriminant,&__discriminant,0,true);
  /* I/O on stream */
#ifdef NSPIRE
  template<class T>
  void readargs_from_stream(nio::ios_base<T> & inf,vecteur & args,GIAC_CONTEXT)
#else
  void readargs_from_stream(istream & inf,vecteur & args,GIAC_CONTEXT)
#endif
  {
    string to_parse;
    char c;
    for (bool notbackslash=true;;){
      inf.get(c);
      if (!inf 
#ifdef NSPIRE
	  || c==char(EOF)
#endif
	  )
	break;
      if ( notbackslash || (c!='\n') )
	to_parse +=c;
      else
	to_parse = to_parse.substr(0,to_parse.size()-1);
      notbackslash= (c!='\\');
    }
    gen e(to_parse,contextptr);
    if (e.type==_VECT)
      args=*e._VECTptr;
    else
      args=makevecteur(e);
  }

#ifdef NSPIRE
  template<class T>
  gen read1arg_from_stream(nio::ios_base<T> & inf,GIAC_CONTEXT)
#else
  gen read1arg_from_stream(istream & inf,GIAC_CONTEXT)
#endif
  {
    string to_parse;
    char c;
    for (bool notbackslash=true;;){
      inf.get(c);
      if (!inf)
	break;
      if ( notbackslash || (c!='\n') )
	to_parse +=c;
      else
	to_parse = to_parse.substr(0,to_parse.size()-1);
      notbackslash= (c!='\\');
    }
    return gen(to_parse,contextptr);
  }

  void readargs(int ARGC, char *ARGV[],vecteur & args,GIAC_CONTEXT){
#ifdef FXCG
    return;
#else
    // first initialize random generator for factorizations
    srand(0);
    //srand(time(NULL));
    string s;
#ifdef NSPIRE
    if (ARGC==0){ // fake, just to instantiate for file
      file bidon("bidon","r");
      readargs_from_stream(bidon,args,contextptr);
    }
#endif
    if (ARGC==1)
      readargs_from_stream(CIN,args,contextptr);
    else {
      if (ARGC==2) {
	if (!secure_run 
#if !defined(__MINGW_H) && !defined(NSPIRE)
	    && access(ARGV[1],R_OK)==0
#endif
	    ){
#if !defined GIAC_HAS_STO_38 && !defined NSPIRE && !defined FXCG && !defined POCKETCAS
	  ifstream inf(ARGV[1]);
	  readargs_from_stream(inf,args,contextptr);
#endif
	}
	else {
	  s=string(ARGV[1]);
	  gen e(s,contextptr);
	  args.push_back(e);
	}
      }
      else {
	vecteur v;
	for (int i=1;i<ARGC;i++){
	  s=string(ARGV[i]);
	  gen e(s,contextptr);
	  v.push_back(e);
	}
	args.push_back(v);
      }
    }
    if (args.empty())
      args.push_back(gentypeerr(contextptr));
#endif
  }

#if 0
  /* Functions below are not used anymore */
  // rewrite embedded fraction inside g as num/den
  static void reduce_alg_ext(const gen & g,gen & num,gen & den){
    num=g;
    den=plus_one;
    int embeddings=0;
    vector<int> embeddings_s;
    for (;((num.type==_POLY) && (Tis_constant<gen>(*num._POLYptr)));++embeddings){
      embeddings_s.push_back(num._POLYptr->dim);
      num=num._POLYptr->coord.front().value;
    }
    if (num.type==_EXT)
      num=ext_reduce(num);
    if (is_undef(num)) return;
    if (num.type==_FRAC){
      den=num._FRACptr->den;
      num=num._FRACptr->num;
    }
    /* else commented since ext_reduce above might reduce g
       else {
      num=g;
      return;
      } */
    for (;embeddings;--embeddings){
      num=polynome(num,embeddings_s.back());
      den=polynome(den,embeddings_s.back());
      embeddings_s.pop_back();
    }    
  }
#endif

  // check in g all ext1 ext and rewrite them with ext2 
  static void remove_ext_copies(gen & g,const gen & ext1,const gen & ext2){
    if (g.type==_POLY){
      vector<monomial<gen> >::iterator it=g._POLYptr->coord.begin(),itend=g._POLYptr->coord.end();
      for (;it!=itend;++it)
	remove_ext_copies(it->value,ext1,ext2);
      return;
    }
    if (g.type==_VECT){
      iterateur it=g._VECTptr->begin(),itend=g._VECTptr->end();
      for (;it!=itend;++it)
	remove_ext_copies(*it,ext1,ext2);
    }
    if (g.type==_FRAC)
      remove_ext_copies(g._FRACptr->num,ext1,ext2);
    if (g.type==_EXT){
      gen & ext=*(g._EXTptr+1);
      if (ext==ext1)
	ext=ext2;
      else
	remove_ext_copies(ext,ext1,ext2);
    }
  }

#ifndef NO_NAMESPACE_GIAC
} // namespace giac
#endif // ndef NO_NAMESPACE_GIAC

