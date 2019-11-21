#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <stdlib.h>

#include <Minuit2/FCNBase.h>
#include <Minuit2/MnMigrad.h>
#include <Minuit2/MnSimplex.h>
#include <Minuit2/MnMinos.h>
#include <Minuit2/MnStrategy.h>
#include <Minuit2/MnUserParameters.h>
#include <Minuit2/FunctionMinimum.h>
#include <TTree.h>
#include <TGraph.h>
#include <TCanvas.h>
#include <TEllipse.h>
#include <TError.h>
#include <TLine.h>
#include <TStyle.h>
#include <TFile.h>
#include <TSystem.h>
#include <TApplication.h>
#include <TH1F.h>
#include <TH2F.h>
#include <TLegend.h>
#include <TPaveText.h>
#include <TText.h>
#include <TGraphAsymmErrors.h>
#include <TROOT.h>
#include <TMath.h>
#include <TProfile.h>
#include <TCanvas.h>

#include "prototype/Utils.hh"
#include "fit/StrawFit.hh"

int main(int argc, char** argv)
{
  std::string filename = "";
  std::string outname = "";
  std::string help = "./efficiency_sim -i <input filename> -o <output filename> [-F (fix position) -f (fermilab setup)]";
  std::string inputcommand = std::string(argv[0]);
  int is_fermilab = 0;
  int fix = 0;
  for (int i=1;i<argc;i++)
    inputcommand += " " + std::string(argv[i]);
  

  int c;
  while ((c = getopt (argc, argv, "Ffhi:o:")) != -1){
    switch (c){
      case 'F':
        fix = 1;
        break;
      case 'f':
        is_fermilab = 1;
        break;
      case 'h':
        std::cout << help << std::endl;
        return 0;
      case 'i':
        filename = std::string(optarg);
        break;
      case 'o':
        outname = std::string(optarg);
        break;
      case '?':
        if (optopt == 'f' || optopt == 'o')
          std::cout << "Option -" << optopt << " requires an argument." << std::endl;
        else
          std::cout << "Unknown option `-" << (char)optopt << "'." << std::endl;
        return 1;
    }
  }

  if (filename.size() == 0 || outname.size() == 0){
    std::cout << help << std::endl;
    return 1;
  }
  
  int argc2 = 0;char **argv2;TApplication theApp("tapp", &argc2, argv2);
 
  gStyle->SetOptFit(1);

  TFile *f = new TFile(filename.c_str());
  TDirectory *d = (TDirectory*) f->Get("makeSD");
  TTree *t = (TTree*) d->Get("swdiag");
  Int_t straw, panel;
  Int_t mcproc[2];
  Int_t mcproccal, mcprochv;
  Float_t xpdist[2];
  Float_t tmin[2];
  Int_t nxing[2];
  t->SetBranchAddress("straw",&straw);
  t->SetBranchAddress("panel",&panel);
  t->SetBranchAddress("mcproc",&mcproc);
  t->SetBranchAddress("xpdist",&xpdist);
  t->SetBranchAddress("tmin",&tmin);
  t->SetBranchAddress("nxing",&nxing);
  TTree *t2 = (TTree*) d->Get("sddiag");
  Int_t straw2, panel2;
  std::vector<unsigned int> *adc;
  adc = 0;
  t2->SetBranchAddress("straw",&straw2);
  t2->SetBranchAddress("panel",&panel2);
  t2->SetBranchAddress("adc",&adc);

  int sddiagcount = 0;
  
  std::vector<bool> has_straw;
  std::vector<bool> has_shower;
  std::vector<double> top;
  std::vector<double> bot;
  std::vector<double> pmp;
  for (int i=0;i<t->GetEntries();i++){
    t->GetEntry(i);
    bool hit = false;
    if (nxing[0] == 1 && nxing[1] == 1){
      hit = true;
    }
    if (sddiagcount < t2->GetEntries())
      t2->GetEntry(sddiagcount);
    if (hit)
      sddiagcount++;
    //FIXME should we cut on mcproc?
//    if (mcproc[0] != 56 || mcproc[1] != 56)
//      continue;
    if (is_fermilab == 0){
      if (straw < 88 || panel != 1)
        continue;
    }else{
      if (straw > 10 || panel != 1)
        continue;
    }
    if (tmin[0] < 300 || tmin[1] < 300)
      continue;
    top.push_back(fabs(xpdist[0])/0.05);
    bot.push_back(fabs(xpdist[0])/0.05);
    if (hit){
      has_straw.push_back(true);
    }else{
      has_straw.push_back(false);
    }
    has_shower.push_back(false);
    if (hit){
      if (straw != straw2 || panel != panel2 || sddiagcount > t2->GetEntries()){
        std::cout << "WARNING: sddiag and swdiag out of sync, peak minus pedestal unusable" << std::endl;
        pmp.push_back(0);
      }else{
        unsigned int peak = 0;
        for (int j=0;j<15;j++){
          peak = std::max(adc->at(j),peak);
        }
        double pedestal = (adc->at(0)+adc->at(1))/2.;
        pmp.push_back(peak-pedestal);
      }
    }else{
      pmp.push_back(0);
    }
  }

  std::cout << top.size() << " events" << std::endl;



  TH1F *tot_efficiency_straw = new TH1F("tot_eff_straw","eff_straw",100,0,5);
  TH1F *tot_efficiency_straw_d = new TH1F("tot_eff_straw_d","eff_straw",100,0,5);

  TH1F *totd_efficiency_straw = new TH1F("totd_eff_straw","eff_straw",120,-1,5);
  TH1F *totd_efficiency_straw_d = new TH1F("totd_eff_straw_d","eff_straw",120,-1,5);

  std::vector<double> strawbestfit;
  std::vector<double> strawbestfiterrors;

  StrawFit strawfcn(top,bot,has_straw,has_shower);

  std::vector<double> strawseed(4,0);
  strawseed[0] = 0.95;
  strawseed[1] = 0.5;
  strawseed[2] = 0.0;
  strawseed[3] = 0.0;
  std::vector<double> strawerrors(4,0.1);

  ROOT::Minuit2::MnUserParameters strawparams(strawseed,strawerrors);
  ROOT::Minuit2::MnMigrad strawmigrad(strawfcn,strawparams);

  if (fix){
    strawmigrad.Fix((unsigned int) 2);
    strawmigrad.Fix((unsigned int) 3);
  }
  strawmigrad.SetLimits((unsigned int) 0,0,1);
  strawmigrad.SetLimits((unsigned int) 1,0,3);
  ROOT::Minuit2::FunctionMinimum strawmin = strawmigrad();
  ROOT::Minuit2::MnUserParameters strawresults = strawmin.UserParameters();
  strawbestfit = strawresults.Params();
  strawbestfiterrors = strawresults.Errors();

  double meaneff = strawbestfit[0];
  double exponent = strawbestfit[1];
  double hdist = strawbestfit[2];
  double vdist = strawbestfit[3];

  TFile *fout = new TFile(outname.c_str(),"RECREATE");
  TTree *tout = new TTree("efficiency","efficiency");
  double t_top, t_bot, t_doca;
  int t_hit, t_shower;
  double t_pmp;
  tout->Branch("top",&t_top,"top/D");
  tout->Branch("bot",&t_bot,"bot/D");
  tout->Branch("doca",&t_doca,"doca/D");
  tout->Branch("pmp",&t_pmp,"pmp/D");
  tout->Branch("hit",&t_hit,"hit/I");
  tout->Branch("shower",&t_shower,"shower/I");
  for (int i=0;i<top.size();i++){
    t_top = top[i];
    t_bot = bot[i];
    t_doca = calculate_DOCA(top[i],bot[i],hdist,vdist);
    t_pmp = pmp[i];
    t_hit = has_straw[i];
    t_shower = has_shower[i];
    tout->Fill();
  }
  tout->Write();
  TTree *tresults = new TTree("efficiencyfit","efficiencyfit");
  double t_meaneff, t_exponent, t_hdist, t_vdist;
  double t_meanefferr, t_exponenterr, t_hdisterr, t_vdisterr;
  tresults->Branch("meaneff",&t_meaneff,"meaneff/D");
  tresults->Branch("exponent",&t_exponent,"exponent/D");
  tresults->Branch("hdist",&t_hdist,"hdist/D");
  tresults->Branch("vdist",&t_vdist,"vdist/D");
  tresults->Branch("meanefferr",&t_meanefferr,"meanefferr/D");
  tresults->Branch("exponenterr",&t_exponenterr,"exponenterr/D");
  tresults->Branch("hdisterr",&t_hdisterr,"hdisterr/D");
  tresults->Branch("vdisterr",&t_vdisterr,"vdisterr/D");
  t_meaneff = strawbestfit[0];
  t_exponent = strawbestfit[1];
  t_hdist = strawbestfit[2];
  t_vdist = strawbestfit[3];
  t_meanefferr = strawbestfiterrors[0];
  t_exponenterr = strawbestfiterrors[1];
  t_hdisterr = strawbestfiterrors[2];
  t_vdisterr = strawbestfiterrors[3];

  tresults->Fill();
  tresults->Write();

  TObjString *tl = new TObjString(inputcommand.c_str());
  fout->WriteObject(tl,"command");
  fout->Close();

  std::cout << "Mean efficiency: " << t_meaneff << " +- " << t_meanefferr << std::endl;
  std::cout << "Exponent: " << t_exponent << " +- " << t_exponenterr << std::endl;
  std::cout << "hdist: " << t_hdist << " +- " << t_hdisterr << std::endl;
  std::cout << "vdist: " << t_vdist << " +- " << t_vdisterr << std::endl;

  TH1F *efficiency_straw = new TH1F("eff_straw","eff_straw",100,0,5);
  TH1F *efficiency_straw_d = new TH1F("eff_straw_d","eff_straw",100,0,5);
  for (int j=0;j<top.size();j++){
    if (!has_shower[j]){
      double straw_dist = calculate_DOCA(top[j],bot[j],hdist,vdist);
      efficiency_straw_d->Fill(straw_dist);
      if (has_straw[j]){
        efficiency_straw->Fill(straw_dist);
      }
    }
  }

  double tot_d = efficiency_straw_d->Integral(1,40);
  double tot_n = efficiency_straw->Integral(1,40);
  std::cout << "AVERAGE: " << tot_n/tot_d << " +- " << 1/tot_d*sqrt(tot_n*(1-tot_n/tot_d)) << std::endl;

  TH1F* hpdf= new TH1F("pdf","pdf",80,0,4);
  for (int j=0;j<hpdf->GetNbinsX();j++){
    double dist = hpdf->GetBinCenter(j+1);
    double pdf_prob = strawbestfit[0] * (1 - 0.5*(1+TMath::Erf((dist-2.5)/(fabs(strawbestfit[1])*sqrt(2)))));
    if (dist > 2.5)
      pdf_prob = 0;
    hpdf->SetBinContent(j+1,pdf_prob);
  }

  //efficiency_straw->Divide(efficiency_straw_d);

  
  for (int i=0;i<efficiency_straw->GetNbinsX();i++){
      double d = efficiency_straw_d->GetBinContent(i+1);
      double n = efficiency_straw->GetBinContent(i+1);
      if (n == 0)
        continue;
      double err = (n/d)*(1-n/d)/d;
      efficiency_straw->SetBinError(i+1,err);
      efficiency_straw->SetBinContent(i+1,n/d);
  }


  efficiency_straw->SetStats(0);
  efficiency_straw->SetTitle("");
  efficiency_straw->GetXaxis()->SetTitle("DOCA to straw center (mm)");
  efficiency_straw->GetYaxis()->SetTitle("Fractional efficiency");
  efficiency_straw->SetLineColor(kBlue);
  efficiency_straw->SetMarkerStyle(22);
  efficiency_straw->SetMarkerColor(kBlue);

  double effavg = tot_n/tot_d;
  double effavgerr = 1/tot_d*sqrt(tot_n*(1-tot_n/tot_d));
  TLine *lavg = new TLine(0,effavg,2.0,effavg);
  lavg->SetLineColor(kBlue);
  lavg->SetLineWidth(2);

  TCanvas *c_eff = new TCanvas("c_eff","c_eff",600,600);
  efficiency_straw->Draw("E");
  hpdf->Draw("hist same");
//  hefficiency[0]->Draw("hist same");
  lavg->Draw();
 
  TLegend *l = new TLegend(0.65,0.65,0.85,0.85);
  l->AddEntry(efficiency_straw,"Data","lepf");
  l->AddEntry(hpdf,"Data fit","l");
  l->AddEntry(lavg,"Average","l");
  l->Draw();

  TPaveText *pt = new TPaveText(0.65,0.35,0.85,0.55,"NDC");
  TText *tt = pt->AddText(TString::Format("#epsilon = %.3f #pm %.3f ns",effavg,effavgerr));
  tt->SetTextColor(kBlue);
  tt->Draw();
  pt->Draw();
  
  //TCanvas *c2 = new TCanvas("c2","c2",600,600);
  //dt_inside->Draw();
  //dt_outside->SetLineColor(kRed);
  //dt_outside->Draw("same");

  theApp.Run();



  return 0;
}
