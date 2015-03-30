#ifndef GUI_MAIN_WINDOW_H_
#define GUI_MAIN_WINDOW_H_

#include <memory>

#include <QMainWindow>
#include <QThread>

#include "application.h"
#include "dove_eye/types.h"
#include "parameters_dialog.h"
#include "video_providers_dialog.h"

namespace Ui {
class MainWindow;
}

namespace gui {

class MainWindow : public QMainWindow {
  Q_OBJECT
 public:
  explicit MainWindow(Application *application, QWidget *parent = nullptr);

  ~MainWindow() override;

 public slots:
  void ChangeArity(const dove_eye::CameraIndex arity);

 private slots:
  void ModifyParameters();
  void VideoProviders();

 private:
  Application *application_;

  std::unique_ptr<Ui::MainWindow> ui_;

  std::unique_ptr<ParametersDialog> parameters_dialog_;
  std::unique_ptr<VideoProvidersDialog> video_providers_dialog_;
};

} // end namespace gui

#endif // GUI_MAIN_WINDOW_H_