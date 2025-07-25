#ifndef INTT_CLUSTER_DRAWER_H
#define INTT_CLUSTER_DRAWER_H

#include <qahtml/SingleCanvasDrawer.h>

class INTTClusterDrawer : public SingleCanvasDrawer
{
public:
  INTTClusterDrawer(std::string const& = "intt_cluster_info");
  virtual ~INTTClusterDrawer();

protected:
  int DrawCanvas() override;
  int MakeCanvas(int=-1, int=-1) override;

  using SingleCanvasDrawer::m_canvas; // base class owned
  using SingleCanvasDrawer::m_name;

private:
  TPad *transparent{nullptr};
  TPad *Pad[4]{nullptr};
  const char* histprefix;
};

#endif//INTT_CLUSTER_DRAWER_H
