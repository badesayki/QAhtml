#include "MicromegasDraw.h"

#include <sPhenixStyle.C>

#include <qahtml/QADrawClient.h>
#include <qahtml/QADrawDB.h>

#include <TCanvas.h>
#include <TDatime.h>
#include <TGraphErrors.h>
#include <TH1.h>
#include <TH2.h>
#include <TProfile.h>
#include <TPad.h>
#include <TROOT.h>
#include <TStyle.h>
#include <TSystem.h>
#include <TText.h>
#include <TLatex.h>
#include <TLegend.h>


#include <boost/format.hpp>

#include <cmath>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>

MicromegasDraw::MicromegasDraw(const std::string &name)
  : QADraw(name)
{
  DBVarInit();
  return;
}

int MicromegasDraw::Draw(const std::string &what)
{
  /* SetsPhenixStyle(); */
  int iret = 0;
  int idraw = 0;
  if (what == "ALL" || what == "CLUSTERS")
    {
      iret += DrawClusterInfo();
      idraw++;
    }
  if (!idraw)
    {
      std::cout << " Unimplemented Drawing option: " << what << std::endl;
      iret = -1;
    }
  return iret;
}

TH1* MicromegasDraw::ClusterAverage(TH2* hist, std::string type)
{
  int nX = hist->GetNbinsX();
  int nY = hist->GetNbinsY();

  auto graph = new TH1F( Form("avg_%s", type.c_str()), "", nX, hist->GetXaxis()->GetXmin(), hist->GetXaxis()->GetXmax());

  std::vector<double> xValues, yAverages;

  for (int i = 1; i <= nX; ++i)
    {
      double sum_y = 0;
      double totalEntries = 0;
      for (int j = 1; j <= nY; ++j)
	{
	  double value = hist->GetBinContent(i, j);
	  double center = hist->GetYaxis()->GetBinCenter(j);
	  sum_y += value * center;
	  totalEntries += value;
	}

      double average;
      if (totalEntries > 0){average = sum_y / totalEntries;} else{average = 0;}
      graph->SetBinContent(i, average);
      const char* label = hist->GetXaxis()->GetBinLabel(i);
      graph->GetXaxis()->SetBinLabel(i, label);
    }

  graph->SetMarkerStyle(8);

  return graph;
}

int MicromegasDraw::MakeCanvas(const std::string &name, int num)
{
  QADrawClient *cl = QADrawClient::instance();
  gStyle->SetOptTitle(1);
  int xsize = cl->GetDisplaySizeX();
  int ysize = cl->GetDisplaySizeY();
  TC[num] = new TCanvas(name.c_str(), (boost::format("Micromegas Plots %d") % num).str().c_str(), -1, 0, (int) (xsize / 1.2) , (int) (ysize / 1.2));
  gSystem->ProcessEvents();

  Pad[num][0] = new TPad((boost::format("mypad%d0") % num).str().c_str(), "put", 0.05, 0.52, 0.45, 0.97, 0);
  Pad[num][1] = new TPad((boost::format("mypad%d1") % num).str().c_str(), "a", 0.5, 0.52, 0.95, 0.97, 0);
  Pad[num][2] = new TPad((boost::format("mypad%d2") % num).str().c_str(), "name", 0.05, 0.02, 0.45, 0.47, 0);
  Pad[num][3] = new TPad((boost::format("mypad%d3") % num).str().c_str(), "here", 0.5, 0.02, 0.95, 0.47, 0);

  for (int i=0; i<4; i++)
    {
      Pad[num][i]->SetTopMargin(0.15);
      Pad[num][i]->SetBottomMargin(0.15);
      Pad[num][i]->SetRightMargin(0.15);
      Pad[num][i]->SetLeftMargin(0.15);
    }

  Pad[num][0]->Draw();
  Pad[num][1]->Draw();
  Pad[num][2]->Draw();
  Pad[num][3]->Draw();

  // this one is used to plot the run number on the canvas
  transparent[num] = new TPad((boost::format("transparent%d") % num).str().c_str(), "this does not show", 0, 0, 1, 1);
  transparent[num]->SetFillStyle(4000);
  transparent[num]->Draw();

  return 0;
}

int MicromegasDraw::DrawClusterInfo()
{
  QADrawClient *cl = QADrawClient::instance();

  auto h_cluster_count_ref = static_cast<TH1*>(cl->getHisto("h_MicromegasClusterQA_clustercount_ref"));
  auto h_cluster_count_found = static_cast<TH1*>(cl->getHisto("h_MicromegasClusterQA_clustercount_found"));
  auto h_cluster_multiplicity_raw = static_cast<TH2*>(cl->getHisto("h_MicromegasClusterQA_cluster_multiplicity"));
  auto h_cluster_size_raw = static_cast<TH2*>(cl->getHisto("h_MicromegasClusterQA_cluster_size"));
  auto h_cluster_charge_raw = static_cast<TH2*>(cl->getHisto("h_MicromegasClusterQA_cluster_charge"));

  if (!h_cluster_count_ref || !h_cluster_count_found || !h_cluster_multiplicity_raw || !h_cluster_size_raw || !h_cluster_charge_raw)
  {
    std::cerr << "Error: One or more histograms could not be retrieved." << std::endl;
    return -1;
  }

  auto efficiency = static_cast<TH1*>(h_cluster_count_found->Clone("efficiency"));
  efficiency->Divide(h_cluster_count_ref);

  auto h_cluster_multiplicity = ClusterAverage(h_cluster_multiplicity_raw, "mult");
  auto h_cluster_size = ClusterAverage(h_cluster_size_raw, "size");
  auto h_cluster_charge = ClusterAverage(h_cluster_charge_raw, "charge");

  if (!TC[0])
  {
    MakeCanvas("ClusterQA", 0);
  }

  TC[0]->cd();
  TC[0]->Clear("D");

  Pad[0][1]->cd();
  h_cluster_multiplicity->SetTitle("Cluster Multiplicity");
  h_cluster_multiplicity->GetXaxis()->SetTitle("Chamber");
  h_cluster_multiplicity->GetYaxis()->SetTitle("Multiplicity");
  h_cluster_multiplicity->SetMinimum(0);
  h_cluster_multiplicity->SetMaximum(10);
  h_cluster_multiplicity->DrawCopy("P");

  Pad[0][2]->cd();
  h_cluster_size->SetTitle("Cluster Size");
  h_cluster_size->GetXaxis()->SetTitle("Chamber");
  h_cluster_size->GetYaxis()->SetTitle("Size");
  h_cluster_size->SetMinimum(0);
  h_cluster_size->SetMaximum(8);
  h_cluster_size->DrawCopy("P");

  Pad[0][0]->cd();
  h_cluster_charge->SetTitle("Cluster Charge");
  h_cluster_charge->GetXaxis()->SetTitle("Chamber");
  h_cluster_charge->GetYaxis()->SetTitle("Charge");
  h_cluster_charge->SetMinimum(0);
  h_cluster_charge->SetMaximum(1000);
  h_cluster_charge->DrawCopy("P");

  Pad[0][3]->cd();
  efficiency->SetMinimum(0);
  efficiency->SetMaximum(1);
  efficiency->SetTitle("Efficiency Estimate by Chamber");
  efficiency->GetXaxis()->SetTitle("Chamber");
  efficiency->GetYaxis()->SetTitle("Efficiency");
  efficiency->DrawCopy();

  TC[0]->Update();

  return 0;
}


int MicromegasDraw::MakeHtml(const std::string &what)
{
  int iret = Draw(what);
  if (iret) // on error no html output please
  {
    return iret;
  }

  auto cl = QADrawClient::instance();
  std::string pngfile;

  // Register the canvas png file to the menu and produces the png file.

  if (what == "ALL" || what == "CLUSTERS")
    {
      pngfile = cl->htmlRegisterPage(*this, "cluster_info", "1", "png");
      cl->CanvasToPng(TC[0], pngfile);
    }


  return 0;
}

int MicromegasDraw::DBVarInit()
{
  /* db = new QADrawDB(this); */
  /* db->DBInit(); */
  return 0;
}
