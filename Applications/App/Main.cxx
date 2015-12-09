/*==============================================================================

  Copyright (c) Kitware, Inc.

  See http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Jean-Christophe Fillion-Robin, Kitware, Inc.
  and was partially funded by NIH grant 3P41RR013218-12S1

==============================================================================*/

// MatlabModules include
#include "NIRFASTSlicerMatlabModulesConfigure.h"

// Qt includes
#include <QDebug>
#include <QList>
#include <QSettings>
#include <QSplashScreen>
#include <QString>
#include <QTimer>
#include <QLabel>
#include <QLineEdit>

// Slicer includes
#include "vtkSlicerConfigure.h"

// CTK includes
#include <ctkAbstractLibraryFactory.h>
#include <ctkCollapsibleButton.h>
#ifdef Slicer_USE_PYTHONQT
# include <ctkPythonConsole.h>
#endif

// Slicer includes
#include "qSlicerAppVersionConfigure.h" // For qSlicerApp_VERSION_FULL, qSlicerApp_VERSION_FULL

// SlicerApp includes
#include "qSlicerApplication.h"
#include "qSlicerApplicationHelper.h"
#ifdef Slicer_BUILD_CLI_SUPPORT
# include "qSlicerCLIExecutableModuleFactory.h"
# include "qSlicerCLILoadableModuleFactory.h"
#endif
#include "qAppMainWindow.h"
#include "qSlicerCommandOptions.h"
#include "qSlicerModuleFactoryManager.h"
#include "qSlicerModuleManager.h"
#include <qSlicerAbstractModuleRepresentation.h>
#include <qSlicerAbstractModuleWidget.h>
#include "Widgets/qAppStyle.h"

// ITK includes
#include <itkFactoryRegistration.h>

// VTK includes
//#include <vtkObject.h>
#include <vtksys/SystemTools.hxx>

#if defined (_WIN32) && !defined (Slicer_BUILD_WIN32_CONSOLE)
# include <windows.h>
# include <vtksys/Encoding.hxx>
#endif

namespace
{

#ifdef Slicer_USE_QtTesting
//-----------------------------------------------------------------------------
void setEnableQtTesting()
{
  if (qSlicerApplication::application()->commandOptions()->enableQtTesting() ||
      qSlicerApplication::application()->userSettings()->value("QtTesting/Enabled").toBool())
    {
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeMenuBar);
    }
}
#endif

//-----------------------------------------------------------------------------
bool isPathWithinPathsList(const QString& dirPath, const QStringList& pathsList)
{
  QDir dirToCheck(dirPath);
  foreach(const QString& path, pathsList)
  {
    if (dirToCheck == QDir(dirPath))
    {
      return true;
    }
  }
  return false;
}

//----------------------------------------------------------------------------
void splashMessage(QScopedPointer<QSplashScreen>& splashScreen, const QString& message)
{
  if (splashScreen.isNull())
    {
    return;
    }
  splashScreen->showMessage(message, Qt::AlignBottom | Qt::AlignHCenter);
}

//----------------------------------------------------------------------------
int SlicerAppMain(int argc, char* argv[])
{
  itk::itkFactoryRegistration();

#if QT_VERSION >= 0x040803
#ifdef Q_OS_MACX
  if (QSysInfo::MacintoshVersion > QSysInfo::MV_10_8)
    {
    // Fix Mac OS X 10.9 (mavericks) font issue
    // https://bugreports.qt-project.org/browse/QTBUG-32789
    QFont::insertSubstitution(".Lucida Grande UI", "Lucida Grande");
    }
#endif
#endif

  // Allow a custom appliction name so that the settings
  // can be distinct for differently named applications
  QString applicationName("Slicer");
  if (argv[0])
    {
    std::string name = vtksys::SystemTools::GetFilenameWithoutExtension(argv[0]);
    applicationName = QString::fromLocal8Bit(name.c_str());
    applicationName.remove(QString("App-real"));
    }
  QCoreApplication::setApplicationName(applicationName);

  QCoreApplication::setApplicationVersion(qSlicerApp_VERSION_FULL);
  QApplication::setDesktopSettingsAware(false);
  QApplication::setStyle(new qAppStyle);

  // Instantiate the settings that are being used everywhere in Slicer.
  QSettings settings(
    QSettings::IniFormat,
    QSettings::UserScope,
    Slicer_ORGANIZATION_NAME,
    Slicer_MAIN_PROJECT_APPLICATION_NAME);

  // Instantiate the built-in application settings located in the resources
  // system.
  QString defaultSettingsFilePath = QString(":/DefaultSettings.ini");
  QSettings defaultSettings(defaultSettingsFilePath, QSettings::IniFormat);
  foreach(const QString& key, defaultSettings.allKeys())
    {
    if (!settings.contains(key))
      {
      settings.setValue(key, defaultSettings.value(key));
      }
    }

  qSlicerApplication app(argc, argv);
  if (app.returnCode() != -1)
    {
    return app.returnCode();
    }

#ifdef Slicer_USE_QtTesting
  setEnableQtTesting(); // disabled the native menu bar.
#endif

#ifdef Slicer_USE_PYTHONQT
  ctkPythonConsole pythonConsole;
  pythonConsole.setWindowTitle("Python Interactor");
  if (!qSlicerApplication::testAttribute(qSlicerApplication::AA_DisablePython))
    {
    qSlicerApplicationHelper::initializePythonConsole(&pythonConsole);
    }
#endif

  bool enableMainWindow = !app.commandOptions()->noMainWindow();
  enableMainWindow = enableMainWindow && !app.commandOptions()->runPythonAndExit();
  bool showSplashScreen = !app.commandOptions()->noSplash() && enableMainWindow;

  QScopedPointer<QSplashScreen> splashScreen;
  if (showSplashScreen)
    {
    QPixmap pixmap(":/SplashScreen.png");
    splashScreen.reset(new QSplashScreen(pixmap));
    splashMessage(splashScreen, "Initializing...");
    splashScreen->show();
    }

  // Append Matlab module path to the additional paths
  QString matlabModulesPath = app.slicerHome() + "/" + MATLABMODULES_DIR;
  QStringList additionalPaths = app.revisionUserSettings()->value("Modules/AdditionalPaths").toStringList();
  if (!isPathWithinPathsList(matlabModulesPath,additionalPaths))
    {
    additionalPaths << matlabModulesPath;
    app.revisionUserSettings()->setValue("Modules/AdditionalPaths",additionalPaths);
    qDebug() << "Adding path to Modules/AdditionalPaths : " << matlabModulesPath.toLatin1();
    }

  // Define ModuleFactoryManager using the additional paths
  qSlicerModuleManager * moduleManager = qSlicerApplication::application()->moduleManager();
  qSlicerModuleFactoryManager * moduleFactoryManager = moduleManager->factoryManager();
  moduleFactoryManager->addSearchPaths(app.commandOptions()->additonalModulePaths());
  qSlicerApplicationHelper::setupModuleFactoryManager(moduleFactoryManager);

  // Register and instantiate modules
  splashMessage(splashScreen, "Registering modules...");
  moduleFactoryManager->registerModules();
  if (app.commandOptions()->verboseModuleDiscovery())
    {
    qDebug() << "Number of registered modules:"
             << moduleFactoryManager->registeredModuleNames().count();
    }
  splashMessage(splashScreen, "Instantiating modules...");
  moduleFactoryManager->instantiateModules();
  if (app.commandOptions()->verboseModuleDiscovery())
    {
    qDebug() << "Number of instantiated modules:"
             << moduleFactoryManager->instantiatedModuleNames().count();
    }
  // Create main window
  splashMessage(splashScreen, "Initializing user interface...");
  QScopedPointer<qAppMainWindow> window;
  if (enableMainWindow)
    {
    window.reset(new qAppMainWindow);
    QString windowTitle = "%1 %2";
    window->setWindowTitle(
      windowTitle.arg(Slicer_MAIN_PROJECT_APPLICATION_NAME).arg(qSlicerApp_VERSION));
    }

  // Load all available modules
  foreach(const QString& name, moduleFactoryManager->instantiatedModuleNames())
    {
    Q_ASSERT(!name.isNull());
    splashMessage(splashScreen, "Loading module \"" + name + "\"...");
    moduleFactoryManager->loadModule(name);
    }
  if (app.commandOptions()->verboseModuleDiscovery())
    {
    qDebug() << "Number of loaded modules:" << moduleManager->modulesNames().count();
    }

  // Edit MatlabModuleGenerator widget
  qSlicerAbstractCoreModule * matlabModuleGenerator = moduleManager->module("MatlabModuleGenerator");
  qSlicerAbstractModuleWidget* matlabWidget = dynamic_cast<qSlicerAbstractModuleWidget*>(matlabModuleGenerator->widgetRepresentation());
  if(matlabWidget)
    {
    QLabel* matlabLabel = matlabWidget->findChild<QLabel*>("label_3");
    if(matlabLabel)
      {
      matlabLabel->hide();
      }
    QLineEdit* matlabLineEdit = matlabWidget->findChild<QLineEdit*>("lineEdit_MatlabScriptDirectory");
    if(matlabLineEdit)
      {
        matlabLineEdit->hide();
      }
    ctkCollapsibleButton* matlabCollapsibleButton = matlabWidget->findChild<ctkCollapsibleButton*>("CollapsibleButton");
    if(matlabCollapsibleButton)
      {
      matlabCollapsibleButton->hide();
      }
    }
  else
    {
    qWarning() << "Could not update UI for the module"<< matlabModuleGenerator->name();
    }

  // Launch NIRFAST-Slicer splashScreen & window
  splashMessage(splashScreen, QString());

  if (window)
    {
    window->setHomeModuleCurrent();
    window->show();
    }

  if (splashScreen && window)
    {
    splashScreen->finish(window.data());
    }

  // Process command line argument after the event loop is started
  QTimer::singleShot(0, &app, SLOT(handleCommandLineArguments()));

  // Look at QApplication::exec() documentation, it is recommended to connect
  // clean up code to the aboutToQuit() signal
  return app.exec();
}

} // end of anonymous namespace

#if defined (_WIN32) && !defined (Slicer_BUILD_WIN32_CONSOLE)
int __stdcall WinMain(HINSTANCE hInstance,
                      HINSTANCE hPrevInstance,
                      LPSTR lpCmdLine, int nShowCmd)
{
  Q_UNUSED(hInstance);
  Q_UNUSED(hPrevInstance);
  Q_UNUSED(nShowCmd);

  // CommandLineToArgvW has no narrow-character version, so we get the arguments in wide strings
  // and then convert to regular string.
  int argc;
  LPWSTR* argvStringW = CommandLineToArgvW(GetCommandLineW(), &argc);

  std::vector< const char* > argv(argc); // usual const char** array used in main() functions
  std::vector< std::string > argvString(argc); // this stores the strings that the argv pointers point to
  for(int i=0; i<argc; i++)
    {
    argvString[i] = vtksys::Encoding::ToNarrow(argvStringW[i]);
    argv[i] = argvString[i].c_str();
    }

  LocalFree(argvStringW);

  return SlicerAppMain(argc, const_cast< char** >(&argv[0]));
}
#else
int main(int argc, char *argv[])
{
  return SlicerAppMain(argc, argv);
}
#endif
