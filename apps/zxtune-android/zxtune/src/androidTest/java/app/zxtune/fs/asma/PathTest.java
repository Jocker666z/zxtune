package app.zxtune.fs.asma;

import android.net.Uri;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import static org.junit.Assert.*;

import org.junit.Test;
import org.junit.runner.RunWith;

import app.zxtune.test.BuildConfig;

// dir/file.sid
@RunWith(AndroidJUnit4.class)
public class PathTest {

  @Test
  public void testEmpty() {
    final Path path = Path.create();
    verifyEmpty(path);
    verifyDir(path.getChild("dir"));
  }

  @Test
  public void testDir() {
    final Path path = Path.parse(Uri.parse("asma:/dir"));
    verifyDir(path);
    verifyEmpty(path.getParent());
    verifyFile(path.getChild("file.sap"));
  }

  @Test
  public void testFile() {
    final Path path = Path.parse(Uri.parse("asma:/dir/file.sap"));
    verifyFile(path);
    verifyDir(path.getParent());
  }

  @Test
  public void testForeign() {
    final Path path = Path.parse(Uri.parse("foreign:/uri/test"));
    assertEquals(null, path);
  }

  private static void verifyEmpty(Path path) {
    final Uri[] uris = path.getRemoteUris();
    assertEquals("getRemoteUris.length", 2, uris.length);
    assertEquals("getRemoteUris[0]", BuildConfig.CDN_ROOT + "/browse/asma/", uris[0].toString());
    assertEquals("getRemoteUris[1]", "http://asma.atari.org/asma/", uris[1].toString());
    assertEquals("getLocalId", "", path.getLocalId());
    assertEquals("getUri", "asma:", path.getUri().toString());
    assertEquals("getName", null, path.getName());
    assertEquals("getParent", null, path.getParent());
    assertTrue("isEmpty", path.isEmpty());
    assertFalse("isFile", path.isFile());
  }

  private static void verifyDir(Path path) {
    final Uri[] uris = path.getRemoteUris();
    assertEquals("getRemoteUris.length", 2, uris.length);
    assertEquals("getRemoteUris[0]", BuildConfig.CDN_ROOT + "/browse/asma/dir/", uris[0].toString());
    assertEquals("getRemoteUris[1]", "http://asma.atari.org/asma/dir/", uris[1].toString());
    assertEquals("getLocalId", "dir", path.getLocalId());
    assertEquals("getUri", "asma:/dir", path.getUri().toString());
    assertEquals("getName", "dir", path.getName());
    assertFalse("isEmpty", path.isEmpty());
    assertFalse("isFile", path.isFile());
  }

  private static void verifyFile(Path path) {
    final Uri[] uris = path.getRemoteUris();
    assertEquals("getRemoteUris.length", 2, uris.length);
    assertEquals("getRemoteUris[0]", BuildConfig.CDN_ROOT + "/browse/asma/dir/file.sap", uris[0].toString());
    assertEquals("getRemoteUris[1]", "http://asma.atari.org/asma/dir/file.sap", uris[1].toString());
    assertEquals("getLocalId", "dir/file.sap", path.getLocalId());
    assertEquals("getUri", "asma:/dir/file.sap", path.getUri().toString());
    assertEquals("getName", "file.sap", path.getName());
    assertFalse("isEmpty", path.isEmpty());
    assertTrue("isFile", path.isFile());
  }
}
