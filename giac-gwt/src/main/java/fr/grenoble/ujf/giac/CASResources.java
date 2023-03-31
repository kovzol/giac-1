package fr.grenoble.ujf.giac;


/* new version (incompatible with GeoGebra Discovery, FIXME)
import org.gwtproject.resources.client.ClientBundle;
import org.gwtproject.resources.client.Resource;
import org.gwtproject.resources.client.TextResource;
*/

/* old version */
import com.google.gwt.core.client.GWT;
import com.google.gwt.resources.client.ClientBundle;
import com.google.gwt.resources.client.TextResource;




/**
 * CAS resource bundle
 */

/* new version
@Resource
*/
public interface CASResources extends ClientBundle {

	/**
	 * maybe it's better if INSTANCE is created later?
	 */
	CASResources INSTANCE = GWT.create(CASResources.class);

	/** @return giac.wasm */
	@Source("fr/grenoble/ujf/giac/giac.wasm.js")
	TextResource giacWasm();

}
